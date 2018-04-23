/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use euclid::{Angle, TypedSize2D};
use parse_function::parse_function;
use std::f32;
use std::str::FromStr;
use webrender::api::*;
use yaml_rust::{Yaml, YamlLoader};

pub trait YamlHelper {
    fn as_f32(&self) -> Option<f32>;
    fn as_force_f32(&self) -> Option<f32>;
    fn as_vec_f32(&self) -> Option<Vec<f32>>;
    fn as_vec_u32(&self) -> Option<Vec<u32>>;
    fn as_vec_u64(&self) -> Option<Vec<u64>>;
    fn as_pipeline_id(&self) -> Option<PipelineId>;
    fn as_rect(&self) -> Option<LayoutRect>;
    fn as_size(&self) -> Option<LayoutSize>;
    fn as_point(&self) -> Option<LayoutPoint>;
    fn as_vector(&self) -> Option<LayoutVector2D>;
    fn as_matrix4d(&self) -> Option<LayoutTransform>;
    fn as_transform(&self, transform_origin: &LayoutPoint) -> Option<LayoutTransform>;
    fn as_colorf(&self) -> Option<ColorF>;
    fn as_vec_colorf(&self) -> Option<Vec<ColorF>>;
    fn as_px_to_au(&self) -> Option<Au>;
    fn as_pt_to_au(&self) -> Option<Au>;
    fn as_vec_string(&self) -> Option<Vec<String>>;
    fn as_border_radius_component(&self) -> LayoutSize;
    fn as_border_radius(&self) -> Option<BorderRadius>;
    fn as_transform_style(&self) -> Option<TransformStyle>;
    fn as_glyph_raster_space(&self) -> Option<GlyphRasterSpace>;
    fn as_clip_mode(&self) -> Option<ClipMode>;
    fn as_mix_blend_mode(&self) -> Option<MixBlendMode>;
    fn as_scroll_policy(&self) -> Option<ScrollPolicy>;
    fn as_filter_op(&self) -> Option<FilterOp>;
    fn as_vec_filter_op(&self) -> Option<Vec<FilterOp>>;
}

fn string_to_color(color: &str) -> Option<ColorF> {
    match color {
        "red" => Some(ColorF::new(1.0, 0.0, 0.0, 1.0)),
        "green" => Some(ColorF::new(0.0, 1.0, 0.0, 1.0)),
        "blue" => Some(ColorF::new(0.0, 0.0, 1.0, 1.0)),
        "white" => Some(ColorF::new(1.0, 1.0, 1.0, 1.0)),
        "black" => Some(ColorF::new(0.0, 0.0, 0.0, 1.0)),
        "yellow" => Some(ColorF::new(1.0, 1.0, 0.0, 1.0)),
        s => {
            let items: Vec<f32> = s.split_whitespace()
                .map(|s| f32::from_str(s).unwrap())
                .collect();
            if items.len() == 3 {
                Some(ColorF::new(
                    items[0] / 255.0,
                    items[1] / 255.0,
                    items[2] / 255.0,
                    1.0,
                ))
            } else if items.len() == 4 {
                Some(ColorF::new(
                    items[0] / 255.0,
                    items[1] / 255.0,
                    items[2] / 255.0,
                    items[3],
                ))
            } else {
                None
            }
        }
    }
}

pub trait StringEnum: Sized {
    fn from_str(&str) -> Option<Self>;
    fn as_str(&self) -> &'static str;
}

macro_rules! define_string_enum {
    ($T:ident, [ $( $y:ident = $x:expr ),* ]) => {
        impl StringEnum for $T {
            fn from_str(text: &str) -> Option<$T> {
                match text {
                $( $x => Some($T::$y), )*
                    _ => {
                        println!("Unrecognized {} value '{}'", stringify!($T), text);
                        None
                    }
                }
            }
            fn as_str(&self) -> &'static str {
                match *self {
                $( $T::$y => $x, )*
                }
            }
        }
    }
}

define_string_enum!(TransformStyle, [Flat = "flat", Preserve3D = "preserve-3d"]);

define_string_enum!(
    MixBlendMode,
    [
        Normal = "normal",
        Multiply = "multiply",
        Screen = "screen",
        Overlay = "overlay",
        Darken = "darken",
        Lighten = "lighten",
        ColorDodge = "color-dodge",
        ColorBurn = "color-burn",
        HardLight = "hard-light",
        SoftLight = "soft-light",
        Difference = "difference",
        Exclusion = "exclusion",
        Hue = "hue",
        Saturation = "saturation",
        Color = "color",
        Luminosity = "luminosity"
    ]
);

define_string_enum!(ScrollPolicy, [Scrollable = "scrollable", Fixed = "fixed"]);

define_string_enum!(
    LineOrientation,
    [Horizontal = "horizontal", Vertical = "vertical"]
);

define_string_enum!(
    LineStyle,
    [
        Solid = "solid",
        Dotted = "dotted",
        Dashed = "dashed",
        Wavy = "wavy"
    ]
);

define_string_enum!(ClipMode, [Clip = "clip", ClipOut = "clip-out"]);

// Rotate around `axis` by `degrees` angle
fn make_rotation(
    origin: &LayoutPoint,
    degrees: f32,
    axis_x: f32,
    axis_y: f32,
    axis_z: f32,
) -> LayoutTransform {
    let pre_transform = LayoutTransform::create_translation(origin.x, origin.y, 0.0);
    let post_transform = LayoutTransform::create_translation(-origin.x, -origin.y, -0.0);

    let theta = 2.0f32 * f32::consts::PI - degrees.to_radians();
    let transform =
        LayoutTransform::identity().pre_rotate(axis_x, axis_y, axis_z, Angle::radians(theta));

    pre_transform.pre_mul(&transform).pre_mul(&post_transform)
}

pub fn make_perspective(
    origin: LayoutPoint,
    perspective: f32,
) -> LayoutTransform {
    let pre_transform = LayoutTransform::create_translation(origin.x, origin.y, 0.0);
    let post_transform = LayoutTransform::create_translation(-origin.x, -origin.y, -0.0);
    let transform = LayoutTransform::create_perspective(perspective);
    pre_transform.pre_mul(&transform).pre_mul(&post_transform)
}

// Create a skew matrix, specified in degrees.
fn make_skew(
    skew_x: f32,
    skew_y: f32,
) -> LayoutTransform {
    let alpha = Angle::radians(skew_x.to_radians());
    let beta = Angle::radians(skew_y.to_radians());
    LayoutTransform::create_skew(alpha, beta)
}

impl YamlHelper for Yaml {
    fn as_f32(&self) -> Option<f32> {
        match *self {
            Yaml::Integer(iv) => Some(iv as f32),
            Yaml::Real(ref sv) => f32::from_str(sv.as_str()).ok(),
            _ => None,
        }
    }

    fn as_force_f32(&self) -> Option<f32> {
        match *self {
            Yaml::Integer(iv) => Some(iv as f32),
            Yaml::String(ref sv) | Yaml::Real(ref sv) => f32::from_str(sv.as_str()).ok(),
            _ => None,
        }
    }

    fn as_vec_f32(&self) -> Option<Vec<f32>> {
        match *self {
            Yaml::String(ref s) | Yaml::Real(ref s) => s.split_whitespace()
                .map(|v| f32::from_str(v))
                .collect::<Result<Vec<_>, _>>()
                .ok(),
            Yaml::Array(ref v) => v.iter()
                .map(|v| match *v {
                    Yaml::Integer(k) => Ok(k as f32),
                    Yaml::String(ref k) | Yaml::Real(ref k) => f32::from_str(k).map_err(|_| false),
                    _ => Err(false),
                })
                .collect::<Result<Vec<_>, _>>()
                .ok(),
            Yaml::Integer(k) => Some(vec![k as f32]),
            _ => None,
        }
    }

    fn as_vec_u32(&self) -> Option<Vec<u32>> {
        if let Some(v) = self.as_vec() {
            Some(v.iter().map(|v| v.as_i64().unwrap() as u32).collect())
        } else {
            None
        }
    }

    fn as_vec_u64(&self) -> Option<Vec<u64>> {
        if let Some(v) = self.as_vec() {
            Some(v.iter().map(|v| v.as_i64().unwrap() as u64).collect())
        } else {
            None
        }
    }

    fn as_pipeline_id(&self) -> Option<PipelineId> {
        if let Some(v) = self.as_vec() {
            let a = v.get(0).and_then(|v| v.as_i64()).map(|v| v as u32);
            let b = v.get(1).and_then(|v| v.as_i64()).map(|v| v as u32);
            match (a, b) {
                (Some(a), Some(b)) if v.len() == 2 => Some(PipelineId(a, b)),
                _ => None,
            }
        } else {
            None
        }
    }

    fn as_px_to_au(&self) -> Option<Au> {
        match self.as_force_f32() {
            Some(fv) => Some(Au::from_f32_px(fv)),
            None => None,
        }
    }

    fn as_pt_to_au(&self) -> Option<Au> {
        match self.as_force_f32() {
            Some(fv) => Some(Au::from_f32_px(fv * 16. / 12.)),
            None => None,
        }
    }

    fn as_rect(&self) -> Option<LayoutRect> {
        if self.is_badvalue() {
            return None;
        }

        if let Some(nums) = self.as_vec_f32() {
            if nums.len() == 4 {
                return Some(LayoutRect::new(
                    LayoutPoint::new(nums[0], nums[1]),
                    LayoutSize::new(nums[2], nums[3]),
                ));
            }
        }

        None
    }

    fn as_size(&self) -> Option<LayoutSize> {
        if self.is_badvalue() {
            return None;
        }

        if let Some(nums) = self.as_vec_f32() {
            if nums.len() == 2 {
                return Some(LayoutSize::new(nums[0], nums[1]));
            }
        }

        None
    }

    fn as_point(&self) -> Option<LayoutPoint> {
        if self.is_badvalue() {
            return None;
        }

        if let Some(nums) = self.as_vec_f32() {
            if nums.len() == 2 {
                return Some(LayoutPoint::new(nums[0], nums[1]));
            }
        }

        None
    }

    fn as_vector(&self) -> Option<LayoutVector2D> {
        self.as_point().map(|p| p.to_vector())
    }

    fn as_matrix4d(&self) -> Option<LayoutTransform> {
        if let Some(nums) = self.as_vec_f32() {
            assert_eq!(nums.len(), 16, "expected 16 floats, got '{:?}'", self);
            Some(LayoutTransform::row_major(
                nums[0],
                nums[1],
                nums[2],
                nums[3],
                nums[4],
                nums[5],
                nums[6],
                nums[7],
                nums[8],
                nums[9],
                nums[10],
                nums[11],
                nums[12],
                nums[13],
                nums[14],
                nums[15],
            ))
        } else {
            None
        }
    }

    fn as_transform(&self, transform_origin: &LayoutPoint) -> Option<LayoutTransform> {
        if let Some(transform) = self.as_matrix4d() {
            return Some(transform);
        }

        match *self {
            Yaml::String(ref string) => {
                let mut slice = string.as_str();
                let mut transform = LayoutTransform::identity();
                while !slice.is_empty() {
                    let (function, ref args, reminder) = parse_function(slice);
                    slice = reminder;
                    let mx = match function {
                        "translate" if args.len() >= 2 => {
                            let z = args.get(2).and_then(|a| a.parse().ok()).unwrap_or(0.);
                            LayoutTransform::create_translation(
                                args[0].parse().unwrap(),
                                args[1].parse().unwrap(),
                                z,
                            )
                        }
                        "rotate" | "rotate-z" if args.len() == 1 => {
                            make_rotation(transform_origin, args[0].parse().unwrap(), 0.0, 0.0, 1.0)
                        }
                        "rotate-x" if args.len() == 1 => {
                            make_rotation(transform_origin, args[0].parse().unwrap(), 1.0, 0.0, 0.0)
                        }
                        "rotate-y" if args.len() == 1 => {
                            make_rotation(transform_origin, args[0].parse().unwrap(), 0.0, 1.0, 0.0)
                        }
                        "scale" if args.len() >= 1 => {
                            let x = args[0].parse().unwrap();
                            // Default to uniform X/Y scale if Y unspecified.
                            let y = args.get(1).and_then(|a| a.parse().ok()).unwrap_or(x);
                            // Default to no Z scale if unspecified.
                            let z = args.get(2).and_then(|a| a.parse().ok()).unwrap_or(1.0);
                            LayoutTransform::create_scale(x, y, z)
                        }
                        "scale-x" if args.len() == 1 => {
                            LayoutTransform::create_scale(args[0].parse().unwrap(), 1.0, 1.0)
                        }
                        "scale-y" if args.len() == 1 => {
                            LayoutTransform::create_scale(1.0, args[0].parse().unwrap(), 1.0)
                        }
                        "scale-z" if args.len() == 1 => {
                            LayoutTransform::create_scale(1.0, 1.0, args[0].parse().unwrap())
                        }
                        "skew" if args.len() >= 1 => {
                            // Default to no Y skew if unspecified.
                            let skew_y = args.get(1).and_then(|a| a.parse().ok()).unwrap_or(0.0);
                            make_skew(args[0].parse().unwrap(), skew_y)
                        }
                        "skew-x" if args.len() == 1 => {
                            make_skew(args[0].parse().unwrap(), 0.0)
                        }
                        "skew-y" if args.len() == 1 => {
                            make_skew(0.0, args[0].parse().unwrap())
                        }
                        "perspective" if args.len() == 1 => {
                            LayoutTransform::create_perspective(args[0].parse().unwrap())
                        }
                        _ => {
                            println!("unknown function {}", function);
                            break;
                        }
                    };
                    transform = transform.post_mul(&mx);
                }
                Some(transform)
            }
            Yaml::Array(ref array) => {
                let transform = array.iter().fold(
                    LayoutTransform::identity(),
                    |u, yaml| match yaml.as_transform(transform_origin) {
                        Some(ref transform) => u.pre_mul(transform),
                        None => u,
                    },
                );
                Some(transform)
            }
            Yaml::BadValue => None,
            _ => {
                println!("unknown transform {:?}", self);
                None
            }
        }
    }

    fn as_colorf(&self) -> Option<ColorF> {
        if let Some(mut nums) = self.as_vec_f32() {
            assert!(
                nums.len() == 3 || nums.len() == 4,
                "color expected a color name, or 3-4 floats; got '{:?}'",
                self
            );

            if nums.len() == 3 {
                nums.push(1.0);
            }
            return Some(ColorF::new(
                nums[0] / 255.0,
                nums[1] / 255.0,
                nums[2] / 255.0,
                nums[3],
            ));
        }

        if let Some(s) = self.as_str() {
            string_to_color(s)
        } else {
            None
        }
    }

    fn as_vec_colorf(&self) -> Option<Vec<ColorF>> {
        if let Some(v) = self.as_vec() {
            Some(v.iter().map(|v| v.as_colorf().unwrap()).collect())
        } else if let Some(color) = self.as_colorf() {
            Some(vec![color])
        } else {
            None
        }
    }

    fn as_vec_string(&self) -> Option<Vec<String>> {
        if let Some(v) = self.as_vec() {
            Some(v.iter().map(|v| v.as_str().unwrap().to_owned()).collect())
        } else if let Some(s) = self.as_str() {
            Some(vec![s.to_owned()])
        } else {
            None
        }
    }

    fn as_border_radius_component(&self) -> LayoutSize {
        if let Yaml::Integer(integer) = *self {
            return LayoutSize::new(integer as f32, integer as f32);
        }
        self.as_size().unwrap_or(TypedSize2D::zero())
    }

    fn as_border_radius(&self) -> Option<BorderRadius> {
        if let Some(size) = self.as_size() {
            return Some(BorderRadius::uniform_size(size));
        }

        match *self {
            Yaml::BadValue => None,
            Yaml::String(ref s) | Yaml::Real(ref s) => {
                let fv = f32::from_str(s).unwrap();
                Some(BorderRadius::uniform(fv))
            }
            Yaml::Integer(v) => Some(BorderRadius::uniform(v as f32)),
            Yaml::Array(ref array) if array.len() == 4 => {
                let top_left = array[0].as_border_radius_component();
                let top_right = array[1].as_border_radius_component();
                let bottom_left = array[2].as_border_radius_component();
                let bottom_right = array[3].as_border_radius_component();
                Some(BorderRadius {
                    top_left,
                    top_right,
                    bottom_left,
                    bottom_right,
                })
            }
            Yaml::Hash(_) => {
                let top_left = self["top-left"].as_border_radius_component();
                let top_right = self["top-right"].as_border_radius_component();
                let bottom_left = self["bottom-left"].as_border_radius_component();
                let bottom_right = self["bottom-right"].as_border_radius_component();
                Some(BorderRadius {
                    top_left,
                    top_right,
                    bottom_left,
                    bottom_right,
                })
            }
            _ => {
                panic!("Invalid border radius specified: {:?}", self);
            }
        }
    }

    fn as_transform_style(&self) -> Option<TransformStyle> {
        self.as_str().and_then(|x| StringEnum::from_str(x))
    }

    fn as_glyph_raster_space(&self) -> Option<GlyphRasterSpace> {
        self.as_str().and_then(|s| {
            match parse_function(s) {
                ("screen", _, _) => {
                    Some(GlyphRasterSpace::Screen)
                }
                ("local", ref args, _) if args.len() == 1 => {
                    Some(GlyphRasterSpace::Local(args[0].parse().unwrap()))
                }
                f => {
                    panic!("error parsing glyph raster space {:?}", f);
                }
            }
        })
    }

    fn as_mix_blend_mode(&self) -> Option<MixBlendMode> {
        self.as_str().and_then(|x| StringEnum::from_str(x))
    }

    fn as_scroll_policy(&self) -> Option<ScrollPolicy> {
        self.as_str().and_then(|x| StringEnum::from_str(x))
    }

    fn as_clip_mode(&self) -> Option<ClipMode> {
        self.as_str().and_then(|x| StringEnum::from_str(x))
    }

    fn as_filter_op(&self) -> Option<FilterOp> {
        if let Some(s) = self.as_str() {
            match parse_function(s) {
                ("blur", ref args, _) if args.len() == 1 => {
                    Some(FilterOp::Blur(args[0].parse().unwrap()))
                }
                ("brightness", ref args, _) if args.len() == 1 => {
                    Some(FilterOp::Brightness(args[0].parse().unwrap()))
                }
                ("contrast", ref args, _) if args.len() == 1 => {
                    Some(FilterOp::Contrast(args[0].parse().unwrap()))
                }
                ("grayscale", ref args, _) if args.len() == 1 => {
                    Some(FilterOp::Grayscale(args[0].parse().unwrap()))
                }
                ("hue-rotate", ref args, _) if args.len() == 1 => {
                    Some(FilterOp::HueRotate(args[0].parse().unwrap()))
                }
                ("invert", ref args, _) if args.len() == 1 => {
                    Some(FilterOp::Invert(args[0].parse().unwrap()))
                }
                ("opacity", ref args, _) if args.len() == 1 => {
                    let amount: f32 = args[0].parse().unwrap();
                    Some(FilterOp::Opacity(amount.into(), amount))
                }
                ("saturate", ref args, _) if args.len() == 1 => {
                    Some(FilterOp::Saturate(args[0].parse().unwrap()))
                }
                ("sepia", ref args, _) if args.len() == 1 => {
                    Some(FilterOp::Sepia(args[0].parse().unwrap()))
                }
                ("drop-shadow", ref args, _) if args.len() == 3 => {
                    let str = format!("---\noffset: {}\nblur-radius: {}\ncolor: {}\n", args[0], args[1], args[2]);
                    let mut yaml_doc = YamlLoader::load_from_str(&str).expect("Failed to parse drop-shadow");
                    let yaml = yaml_doc.pop().unwrap();
                    Some(FilterOp::DropShadow(yaml["offset"].as_vector().unwrap(),
                                              yaml["blur-radius"].as_f32().unwrap(),
                                              yaml["color"].as_colorf().unwrap()))
                }
                ("color-matrix", ref args, _) if args.len() == 20 => {
                    let m: Vec<f32> = args.iter().map(|f| f.parse().unwrap()).collect();
                    let mut matrix: [f32; 20] = [0.0; 20];
                    matrix.clone_from_slice(&m);
                    Some(FilterOp::ColorMatrix(matrix))
                }
                (_, _, _) => None,
            }
        } else {
            None
        }
    }

    fn as_vec_filter_op(&self) -> Option<Vec<FilterOp>> {
        if let Some(v) = self.as_vec() {
            Some(v.iter().map(|x| x.as_filter_op().unwrap()).collect())
        } else {
            self.as_filter_op().map(|op| vec![op])
        }
    }
}
