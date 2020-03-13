/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate cc;
extern crate glsl_to_cxx;

use std::collections::HashSet;
use std::fmt::Write;

fn write_load_shader(shaders: &[&str]) {
    let mut load_shader = String::new();
    for s in shaders {
        let _ = write!(load_shader, "#include \"{}.h\"\n", s);
    }
    load_shader.push_str("ProgramLoader load_shader(const char* name) {\n");
    for s in shaders {
        let _ = write!(load_shader, "  if (!strcmp(name, \"{}\")) {{ return {}_program::loader; }}\n", s, s);
    }
    load_shader.push_str("  return nullptr;\n}\n");
    std::fs::write(std::env::var("OUT_DIR").unwrap() + "/load_shader.h", load_shader).unwrap();
}

fn process_imports(shader_dir: &str, shader: &str, included: &mut HashSet<String>, output: &mut String) {
    if !included.insert(shader.into()) {
        return;
    }
    println!("cargo:rerun-if-changed={}/{}.glsl", shader_dir, shader);
    let source = std::fs::read_to_string(format!("{}/{}.glsl", shader_dir, shader)).unwrap();
    for line in source.lines() {
        if line.starts_with("#include ") {
            let imports = line["#include ".len() ..].split(',');
            for import in imports {
                process_imports(shader_dir, import, included, output);
            }
        } else if line.starts_with("#version ") || line.starts_with("#extension ") {
            // ignore
        } else {
            output.push_str(line);
            output.push('\n');
        }
    }
}

fn translate_shader(shader: &str, shader_dir: &str) {
    let mut imported = String::new();
    imported.push_str("#define SWGL 1\n");
    imported.push_str("#define WR_MAX_VERTEX_TEXTURE_WIDTH 1024U\n");
    let basename = if let Some(feature_start) = shader.find(char::is_uppercase) {
        let feature_end = shader.rfind(char::is_uppercase).unwrap();
        let features = shader[feature_start .. feature_end + 1].split('.');
        for feature in features {
            let _ = write!(imported, "#define WR_FEATURE_{}\n", feature);
        }
        &shader[0..feature_start]
    } else {
        shader
    };

    process_imports(shader_dir, basename, &mut HashSet::new(), &mut imported);

    let out_dir = std::env::var("OUT_DIR").unwrap();
    let imp_name = format!("{}/{}.c", out_dir, shader);
    std::fs::write(&imp_name, imported).unwrap();

    let mut build = cc::Build::new();
    if build.get_compiler().is_like_msvc() {
        build.flag("/EP");
    } else {
        build.flag("-xc").flag("-P");
    }
    build.file(&imp_name);
    let vs = build.clone()
        .define("WR_VERTEX_SHADER", Some("1"))
        .expand();
    let fs = build.clone()
        .define("WR_FRAGMENT_SHADER", Some("1"))
        .expand();
    let vs_name = format!("{}/{}.vert", out_dir, shader);
    let fs_name = format!("{}/{}.frag", out_dir, shader);
    std::fs::write(&vs_name, vs).unwrap();
    std::fs::write(&fs_name, fs).unwrap();

    let mut args = vec![
        "glsl_to_cxx".to_string(),
        vs_name,
        fs_name,
    ];
    let frag_include = format!("{}/{}.frag.h", shader_dir, shader);
    if std::path::Path::new(&frag_include).exists() {
        println!("cargo:rerun-if-changed={}/{}.frag.h", shader_dir, shader);
        args.push(frag_include);
    }
    let result = glsl_to_cxx::translate(&mut args.into_iter());
    std::fs::write(format!("{}/{}.h", out_dir, shader), result).unwrap();
}

const WR_SHADERS: &'static [&'static str] = &[
    "brush_blendALPHA_PASS",
    "brush_blend",
    "brush_imageALPHA_PASS",
    "brush_image",
    "brush_imageREPETITION_ANTIALIASING_ALPHA_PASS",
    "brush_imageREPETITION_ANTIALIASING",
    "brush_linear_gradientALPHA_PASS",
    "brush_linear_gradientDITHERING_ALPHA_PASS",
    "brush_linear_gradientDITHERING",
    "brush_linear_gradient",
    "brush_mix_blendALPHA_PASS",
    "brush_mix_blend",
    "brush_opacityALPHA_PASS",
    "brush_radial_gradientALPHA_PASS",
    "brush_radial_gradientDITHERING_ALPHA_PASS",
    "brush_radial_gradientDITHERING",
    "brush_radial_gradient",
    "brush_solidALPHA_PASS",
    "brush_solid",
    "brush_yuv_image",
    "brush_yuv_imageTEXTURE_2D_YUV_NV12",
    "brush_yuv_imageYUV",
    "brush_yuv_imageYUV_INTERLEAVED",
    "brush_yuv_imageYUV_NV12_ALPHA_PASS",
    "brush_yuv_imageYUV_NV12",
    "brush_yuv_imageYUV_PLANAR",
    "composite",
    "cs_blurALPHA_TARGET",
    "cs_blurCOLOR_TARGET",
    "cs_border_segment",
    "cs_border_solid",
    "cs_clip_box_shadow",
    "cs_clip_image",
    "cs_clip_rectangleFAST_PATH",
    "cs_clip_rectangle",
    "cs_gradient",
    "cs_line_decoration",
    "cs_scale",
    "cs_svg_filter",
    "debug_color",
    "debug_font",
    "ps_split_composite",
    "ps_text_runDUAL_SOURCE_BLENDING",
    "ps_text_runGLYPH_TRANSFORM",
    "ps_text_runDUAL_SOURCE_BLENDING_GLYPH_TRANSFORM",
    "ps_text_run",
];

fn main() {
    let shader_dir = match std::env::var("MOZ_SRC") {
        Ok(dir) => dir + "/gfx/wr/webrender/res",
        Err(_) => std::env::var("CARGO_MANIFEST_DIR").unwrap() + "/../webrender/res",
    };

    for shader in WR_SHADERS {
        translate_shader(shader, &shader_dir);
    }

    write_load_shader(WR_SHADERS);

    println!("cargo:rerun-if-changed=src/gl_defs.h");
    println!("cargo:rerun-if-changed=src/glsl.h");
    println!("cargo:rerun-if-changed=src/program.h");
    println!("cargo:rerun-if-changed=src/texture.h");
    println!("cargo:rerun-if-changed=src/vector_type.h");
    println!("cargo:rerun-if-changed=src/gl.cc");
    cc::Build::new()
        .cpp(true)
        .file("src/gl.cc")
        .flag("-std=c++14")
        .flag("-UMOZILLA_CONFIG_H")
        .flag("-fno-exceptions")
        .flag("-fno-rtti")
        .flag("-fno-math-errno")
        .define("_GLIBCXX_USE_CXX11_ABI", Some("0"))
        .include(shader_dir)
        .include("src")
        .include(std::env::var("OUT_DIR").unwrap())
        .compile("gl_cc");
}
