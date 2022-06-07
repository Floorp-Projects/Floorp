/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use webrender::shader_source::OPTIMIZED_SHADERS;
use glsl::parser::Parse as _;
use glsl::syntax::{InterpolationQualifier, ShaderStage, SingleDeclaration};
use glsl::syntax::{TypeSpecifierNonArray, TypeQualifierSpec};
use glsl::visitor::{Host, Visit, Visitor};

/// Tests that a shader contains no flat scalar varyings.
/// These must be avoided on Adreno 3xx devices due to bug 1630356.
fn test_no_flat_scalar_varyings(name: &str, shader: &mut ShaderStage) {
    struct FlatScalarVaryingsVisitor {
        shader_name: String,
    }

    impl Visitor for FlatScalarVaryingsVisitor {
        fn visit_single_declaration(&mut self, declaration: &SingleDeclaration) -> Visit {
            let is_scalar = matches!(declaration.ty.ty.ty,
                TypeSpecifierNonArray::Bool
                | TypeSpecifierNonArray::Int
                | TypeSpecifierNonArray::UInt
                | TypeSpecifierNonArray::Float
                | TypeSpecifierNonArray::Double);

            let qualifiers = declaration
                .ty
                .qualifier
                .as_ref()
                .map(|q| q.qualifiers.0.as_slice())
                .unwrap_or(&[]);

            let is_flat = qualifiers.contains(&TypeQualifierSpec::Interpolation(
                InterpolationQualifier::Flat,
            ));

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

pub fn test_shaders() {
    for ((_version, name), shader) in OPTIMIZED_SHADERS.iter() {
        let mut vert = ShaderStage::parse(shader.vert_source).unwrap();
        let mut frag = ShaderStage::parse(shader.frag_source).unwrap();

        if cfg!(target_os = "android") {
            test_no_flat_scalar_varyings(&format!("{}.vert", name), &mut vert);
            test_no_flat_scalar_varyings(&format!("{}.frag", name), &mut frag);
        }
    }
}
