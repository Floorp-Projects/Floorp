/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{
    ColorU, FilterOp, LayoutRect, LayoutSize, LayoutPrimitiveInfo, MixBlendMode,
    PropertyBinding, PropertyBindingId,
};
use app_units::Au;
use display_list_flattener::{AsInstanceKind, IsVisible};
use intern::{DataStore, Handle, Internable, Interner, InternDebug, UpdateList};
use picture::PictureCompositeMode;
use prim_store::{
    PrimKey, PrimKeyCommonData, PrimTemplate, PrimTemplateCommonData,
    PrimitiveInstanceKind, PrimitiveSceneData, PrimitiveStore, VectorKey,
};

/// Represents a hashable description of how a picture primitive
/// will be composited into its parent.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, PartialEq, Hash, Eq)]
pub enum PictureCompositeKey {
    // No visual compositing effect
    Identity,

    // FilterOp
    Blur(Au),
    Brightness(Au),
    Contrast(Au),
    Grayscale(Au),
    HueRotate(Au),
    Invert(Au),
    Opacity(Au),
    OpacityBinding(PropertyBindingId, Au),
    Saturate(Au),
    Sepia(Au),
    DropShadow(VectorKey, Au, ColorU),
    ColorMatrix([Au; 20]),
    SrgbToLinear,
    LinearToSrgb,

    // MixBlendMode
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity,
}

impl From<Option<PictureCompositeMode>> for PictureCompositeKey {
    fn from(mode: Option<PictureCompositeMode>) -> Self {
        match mode {
            Some(PictureCompositeMode::MixBlend(mode)) => {
                match mode {
                    MixBlendMode::Normal => PictureCompositeKey::Identity,
                    MixBlendMode::Multiply => PictureCompositeKey::Multiply,
                    MixBlendMode::Screen => PictureCompositeKey::Screen,
                    MixBlendMode::Overlay => PictureCompositeKey::Overlay,
                    MixBlendMode::Darken => PictureCompositeKey::Darken,
                    MixBlendMode::Lighten => PictureCompositeKey::Lighten,
                    MixBlendMode::ColorDodge => PictureCompositeKey::ColorDodge,
                    MixBlendMode::ColorBurn => PictureCompositeKey::ColorBurn,
                    MixBlendMode::HardLight => PictureCompositeKey::HardLight,
                    MixBlendMode::SoftLight => PictureCompositeKey::SoftLight,
                    MixBlendMode::Difference => PictureCompositeKey::Difference,
                    MixBlendMode::Exclusion => PictureCompositeKey::Exclusion,
                    MixBlendMode::Hue => PictureCompositeKey::Hue,
                    MixBlendMode::Saturation => PictureCompositeKey::Saturation,
                    MixBlendMode::Color => PictureCompositeKey::Color,
                    MixBlendMode::Luminosity => PictureCompositeKey::Luminosity,
                }
            }
            Some(PictureCompositeMode::Filter(op)) => {
                match op {
                    FilterOp::Blur(value) => PictureCompositeKey::Blur(Au::from_f32_px(value)),
                    FilterOp::Brightness(value) => PictureCompositeKey::Brightness(Au::from_f32_px(value)),
                    FilterOp::Contrast(value) => PictureCompositeKey::Contrast(Au::from_f32_px(value)),
                    FilterOp::Grayscale(value) => PictureCompositeKey::Grayscale(Au::from_f32_px(value)),
                    FilterOp::HueRotate(value) => PictureCompositeKey::HueRotate(Au::from_f32_px(value)),
                    FilterOp::Invert(value) => PictureCompositeKey::Invert(Au::from_f32_px(value)),
                    FilterOp::Saturate(value) => PictureCompositeKey::Saturate(Au::from_f32_px(value)),
                    FilterOp::Sepia(value) => PictureCompositeKey::Sepia(Au::from_f32_px(value)),
                    FilterOp::SrgbToLinear => PictureCompositeKey::SrgbToLinear,
                    FilterOp::LinearToSrgb => PictureCompositeKey::LinearToSrgb,
                    FilterOp::Identity => PictureCompositeKey::Identity,
                    FilterOp::DropShadow(offset, radius, color) => {
                        PictureCompositeKey::DropShadow(offset.into(), Au::from_f32_px(radius), color.into())
                    }
                    FilterOp::Opacity(binding, _) => {
                        match binding {
                            PropertyBinding::Value(value) => {
                                PictureCompositeKey::Opacity(Au::from_f32_px(value))
                            }
                            PropertyBinding::Binding(key, default) => {
                                PictureCompositeKey::OpacityBinding(key.id, Au::from_f32_px(default))
                            }
                        }
                    }
                    FilterOp::ColorMatrix(values) => {
                        let mut quantized_values: [Au; 20] = [Au(0); 20];
                        for (value, result) in values.iter().zip(quantized_values.iter_mut()) {
                            *result = Au::from_f32_px(*value);
                        }
                        PictureCompositeKey::ColorMatrix(quantized_values)
                    }
                }
            }
            Some(PictureCompositeMode::Blit) |
            Some(PictureCompositeMode::TileCache { .. }) |
            None => {
                PictureCompositeKey::Identity
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Picture {
    pub composite_mode_key: PictureCompositeKey,
}

pub type PictureKey = PrimKey<Picture>;

impl PictureKey {
    pub fn new(
        is_backface_visible: bool,
        prim_size: LayoutSize,
        prim_relative_clip_rect: LayoutRect,
        pic: Picture,
    ) -> Self {

        PictureKey {
            common: PrimKeyCommonData {
                is_backface_visible,
                prim_size: prim_size.into(),
                prim_relative_clip_rect: prim_relative_clip_rect.into(),
            },
            kind: pic,
        }
    }
}

impl InternDebug for PictureKey {}

impl AsInstanceKind<PictureDataHandle> for PictureKey {
    /// Construct a primitive instance that matches the type
    /// of primitive key.
    fn as_instance_kind(
        &self,
        _: PictureDataHandle,
        _: &mut PrimitiveStore,
    ) -> PrimitiveInstanceKind {
        // Should never be hit as this method should not be
        // called for pictures.
        unreachable!();
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureData;

pub type PictureTemplate = PrimTemplate<PictureData>;

impl From<PictureKey> for PictureTemplate {
    fn from(key: PictureKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(key.common);

        PictureTemplate {
            common,
            kind: PictureData,
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub struct PictureDataMarker;

pub type PictureDataStore = DataStore<PictureKey, PictureTemplate, PictureDataMarker>;
pub type PictureDataHandle = Handle<PictureDataMarker>;
pub type PictureDataUpdateList = UpdateList<PictureKey>;
pub type PictureDataInterner = Interner<PictureKey, PrimitiveSceneData, PictureDataMarker>;

impl Internable for Picture {
    type Marker = PictureDataMarker;
    type Source = PictureKey;
    type StoreData = PictureTemplate;
    type InternData = PrimitiveSceneData;

    /// Build a new key from self with `info`.
    fn build_key(
        self,
        info: &LayoutPrimitiveInfo,
        prim_relative_clip_rect: LayoutRect,
    ) -> PictureKey {
        PictureKey::new(
            info.is_backface_visible,
            info.rect.size,
            prim_relative_clip_rect,
            self
        )
    }
}

impl IsVisible for Picture {
    fn is_visible(&self) -> bool {
        true
    }
}

#[test]
#[cfg(target_os = "linux")]
fn test_struct_sizes() {
    use std::mem;
    // The sizes of these structures are critical for performance on a number of
    // talos stress tests. If you get a failure here on CI, there's two possibilities:
    // (a) You made a structure smaller than it currently is. Great work! Update the
    //     test expectations and move on.
    // (b) You made a structure larger. This is not necessarily a problem, but should only
    //     be done with care, and after checking if talos performance regresses badly.
    assert_eq!(mem::size_of::<Picture>(), 84, "Picture size changed");
    assert_eq!(mem::size_of::<PictureTemplate>(), 56, "PictureTemplate size changed");
    assert_eq!(mem::size_of::<PictureKey>(), 112, "PictureKey size changed");
}
