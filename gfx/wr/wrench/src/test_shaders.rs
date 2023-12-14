/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use webrender::ShaderKind;
use webrender_build::shader::{ShaderFeatureFlags, ShaderVersion, build_shader_strings};
use webrender_build::shader::get_shader_features;
use glsl_lang::ast::{InterpolationQualifierData, NodeContent, SingleDeclaration};
use glsl_lang::ast::{StorageQualifierData, TranslationUnit, TypeSpecifierNonArrayData};
use glsl_lang::ast::TypeQualifierSpecData;
use glsl_lang::parse::DefaultParse as _;
use glsl_lang::visitor::{Host, Visit, Visitor};

/// Tests that a shader contains no flat scalar varyings.
/// These must be avoided on Adreno 3xx devices due to bug 1630356.
fn test_no_flat_scalar_varyings(
    name: &str,
    shader: &mut TranslationUnit,
    _shader_kind: ShaderKind,
) {
    struct FlatScalarVaryingsVisitor {
        shader_name: String,
    }

    impl Visitor for FlatScalarVaryingsVisitor {
        fn visit_single_declaration(&mut self, declaration: &SingleDeclaration) -> Visit {
            let is_scalar = matches!(
                declaration.ty.ty.ty.content,
                TypeSpecifierNonArrayData::Bool
                    | TypeSpecifierNonArrayData::Int
                    | TypeSpecifierNonArrayData::UInt
                    | TypeSpecifierNonArrayData::Float
                    | TypeSpecifierNonArrayData::Double
            );

            let qualifiers = declaration
                .ty
                .qualifier
                .as_ref()
                .map(|q| q.qualifiers.as_slice())
                .unwrap_or(&[]);

            let is_flat = qualifiers.contains(
                &TypeQualifierSpecData::Interpolation(InterpolationQualifierData::Flat.into_node())
                    .into_node(),
            );

            assert!(
                !(is_scalar && is_flat),
                "{}: {} is a flat scalar varying",
                self.shader_name,
                &declaration.name.as_ref().unwrap()
            );

            Visit::Parent
        }
    }

    let mut visitor = FlatScalarVaryingsVisitor {
        shader_name: name.to_string(),
    };
    shader.visit(&mut visitor);
}

/// Tests that a shader's varyings have an explicit precision specifier.
/// Mali vendor tooling shows us that we are often varying-iterpolation bound, so using mediump
/// where possible helps alleviate this. By enforcing that varyings are given explicit precisions,
/// we ensure that highp is only used when necessary rather than just by default.
fn test_varying_explicit_precision(
    name: &str,
    shader: &mut TranslationUnit,
    shader_kind: ShaderKind,
) {
    struct VaryingExplicitPrecisionVisitor {
        shader_name: String,
        shader_kind: ShaderKind,
    }

    impl Visitor for VaryingExplicitPrecisionVisitor {
        fn visit_single_declaration(&mut self, declaration: &SingleDeclaration) -> Visit {
            let qualifiers = declaration
                .ty
                .qualifier
                .as_ref()
                .map(|q| q.qualifiers.as_slice())
                .unwrap_or(&[]);

            let is_varying = qualifiers.iter().any(|qualifier| {
                match &qualifier.content {
                    TypeQualifierSpecData::Storage(storage) => match self.shader_kind {
                        ShaderKind::Vertex => storage.content == StorageQualifierData::Out,
                        ShaderKind::Fragment => storage.content == StorageQualifierData::In,
                    }
                    _ => false,
                }
            });

            let has_explicit_precision = qualifiers
                .iter()
                .any(|qualifier| matches!(qualifier.content, TypeQualifierSpecData::Precision(_)));

            assert!(
                !is_varying || has_explicit_precision,
                "{}: {} is a varying without an explicit precision declared",
                self.shader_name,
                &declaration.name.as_ref().unwrap()
            );

            Visit::Parent
        }
    }

    let mut visitor = VaryingExplicitPrecisionVisitor {
        shader_name: name.to_string(),
        shader_kind,
    };
    shader.visit(&mut visitor);
}

pub fn test_shaders() {
    let mut flags = ShaderFeatureFlags::all();
    if cfg!(any(target_os = "windows", target_os = "android")) {
        flags.remove(ShaderFeatureFlags::GL);
    } else {
        flags.remove(ShaderFeatureFlags::GLES);
    }
    // glsl-lang crate fails to parse advanced blend shaders
    flags.remove(ShaderFeatureFlags::ADVANCED_BLEND_EQUATION);
    // glsl-lang crate fails to parse texture external BT709 shaders
    flags.remove(ShaderFeatureFlags::TEXTURE_EXTERNAL_BT709);

    for (shader, configs) in get_shader_features(flags) {
        for config in configs {
            let name = if config.is_empty() {
                shader.to_string()
            } else {
                format!("{}_{}", shader, config.replace(",", "_"))
            };
            let vert_name = format!("{}.vert", name);
            let frag_name = format!("{}.frag", name);


            let features = config
                .split(",")
                .filter(|f| !f.is_empty())
                .collect::<Vec<_>>();

            let (vert_src, frag_src) =
                build_shader_strings(ShaderVersion::Gles, &features, shader, &|f| {
                    webrender::get_unoptimized_shader_source(f, None)
                });

            let mut vert = TranslationUnit::parse(&vert_src).unwrap();
            let mut frag = TranslationUnit::parse(&frag_src).unwrap();


            test_no_flat_scalar_varyings(&vert_name, &mut vert, ShaderKind::Vertex);
            test_no_flat_scalar_varyings(&frag_name, &mut frag, ShaderKind::Fragment);
            test_varying_explicit_precision(&vert_name, &mut vert, ShaderKind::Vertex);
            test_varying_explicit_precision(&frag_name, &mut frag, ShaderKind::Fragment);
        }
    }
}
