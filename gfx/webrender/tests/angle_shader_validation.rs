extern crate angle;
#[macro_use]
extern crate lazy_static;
extern crate webrender;

use angle::hl::{BuiltInResources, Output, ShaderSpec, ShaderValidator};

include!(concat!(env!("OUT_DIR"), "/shaders.rs"));


// from glslang
const FRAGMENT_SHADER: u32 = 0x8B30;
const VERTEX_SHADER: u32 = 0x8B31;


#[test]
fn validate_shaders() {
    angle::hl::initialize().unwrap();

    let shared_src = SHADERS.get("shared").unwrap();
    let prim_shared_src = SHADERS.get("prim_shared").unwrap();
    let clip_shared_src = SHADERS.get("clip_shared").unwrap();

    for (filename, file_source) in SHADERS.iter() {
        let is_prim = filename.starts_with("ps_");
        let is_clip = filename.starts_with("cs_");
        let is_vert = filename.ends_with(".vs");
        let is_frag = filename.ends_with(".fs");
        if !(is_prim ^ is_clip) || !(is_vert ^ is_frag) {
            continue;
        }


        let base_filename = filename.splitn(2, '.').next().unwrap();
        let mut shader_prefix = format!("#version 300 es\n
            // Base shader: {}\n
            #define WR_MAX_VERTEX_TEXTURE_WIDTH {}\n",
            base_filename, webrender::renderer::MAX_VERTEX_TEXTURE_WIDTH);

        if is_vert {
            shader_prefix.push_str("#define WR_VERTEX_SHADER\n");
        } else {
            shader_prefix.push_str("#define WR_FRAGMENT_SHADER\n");
        }

        let mut build_configs = vec!["#define WR_FEATURE_TRANSFORM\n"];
        if is_prim {
            // the transform feature may be disabled for the prim shaders
            build_configs.push("// WR_FEATURE_TRANSFORM disabled\n");
        }

        for config_prefix in build_configs {
            let mut shader_source = String::new();
            shader_source.push_str(shader_prefix.as_str());
            shader_source.push_str(config_prefix);
            shader_source.push_str(shared_src);
            shader_source.push_str(prim_shared_src);
            if is_clip {
                shader_source.push_str(clip_shared_src);
            }
            if let Some(optional_src) = SHADERS.get(base_filename) {
                shader_source.push_str(optional_src);
            }
            shader_source.push_str(file_source);


            let gl_type = if is_vert { VERTEX_SHADER } else { FRAGMENT_SHADER };
            let resources = BuiltInResources::default();
            let validator = ShaderValidator::new(gl_type,
                                                 ShaderSpec::Gles3,
                                                 Output::Essl,
                                                 &resources).unwrap();

            match validator.compile_and_translate(&[&shader_source]) {
                Ok(_) => {
                    println!("Shader translated succesfully: {}", filename);
                },
                Err(_) => {
                    panic!("Shader compilation failed: {}\n{}",
                        filename, validator.info_log());
                },
            }
        }
    }
}
