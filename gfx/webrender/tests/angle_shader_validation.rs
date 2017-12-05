extern crate angle;
extern crate webrender;

use angle::hl::{BuiltInResources, Output, ShaderSpec, ShaderValidator};

// from glslang
const FRAGMENT_SHADER: u32 = 0x8B30;
const VERTEX_SHADER: u32 = 0x8B31;

struct Shader {
    name: &'static str,
    features: &'static [&'static str],
}

const SHADER_PREFIX: &str = "#define WR_MAX_VERTEX_TEXTURE_WIDTH 1024\n";

const CLIP_FEATURES: &[&str] = &["TRANSFORM"];
const CACHE_FEATURES: &[&str] = &[""];
const PRIM_FEATURES: &[&str] = &["", "TRANSFORM"];

const SHADERS: &[Shader] = &[
    // Clip mask shaders
    Shader {
        name: "cs_clip_rectangle",
        features: CLIP_FEATURES,
    },
    Shader {
        name: "cs_clip_image",
        features: CLIP_FEATURES,
    },
    Shader {
        name: "cs_clip_border",
        features: CLIP_FEATURES,
    },
    // Cache shaders
    Shader {
        name: "cs_blur",
        features: CACHE_FEATURES,
    },
    Shader {
        name: "cs_text_run",
        features: CACHE_FEATURES,
    },
    // Prim shaders
    Shader {
        name: "ps_line",
        features: &["", "TRANSFORM", "CACHE"],
    },
    Shader {
        name: "ps_border_corner",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_border_edge",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_gradient",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_angle_gradient",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_radial_gradient",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_blend",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_composite",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_hardware_composite",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_split_composite",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_image",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_yuv_image",
        features: PRIM_FEATURES,
    },
    Shader {
        name: "ps_text_run",
        features: PRIM_FEATURES,
    },
    // Brush shaders
    Shader {
        name: "brush_mask",
        features: &[],
    },
    Shader {
        name: "brush_solid",
        features: &[],
    },
    Shader {
        name: "brush_image",
        features: &["COLOR_TARGET", "ALPHA_TARGET"],
    },
];

const VERSION_STRING: &str = "#version 300 es\n";

#[test]
fn validate_shaders() {
    angle::hl::initialize().unwrap();

    let resources = BuiltInResources::default();
    let vs_validator =
        ShaderValidator::new(VERTEX_SHADER, ShaderSpec::Gles3, Output::Essl, &resources).unwrap();

    let fs_validator =
        ShaderValidator::new(FRAGMENT_SHADER, ShaderSpec::Gles3, Output::Essl, &resources).unwrap();

    for shader in SHADERS {
        for config in shader.features {
            let mut features = String::new();
            features.push_str(SHADER_PREFIX);

            for feature in config.split(",") {
                features.push_str(&format!("#define WR_FEATURE_{}", feature));
            }

            let (vs, fs) =
                webrender::build_shader_strings(VERSION_STRING, &features, shader.name, &None);

            validate(&vs_validator, shader.name, vs);
            validate(&fs_validator, shader.name, fs);
        }
    }
}

fn validate(validator: &ShaderValidator, name: &str, source: String) {
    match validator.compile_and_translate(&[&source]) {
        Ok(_) => {
            println!("Shader translated succesfully: {}", name);
        }
        Err(_) => {
            panic!(
                "Shader compilation failed: {}\n{}",
                name,
                validator.info_log()
            );
        }
    }
}
