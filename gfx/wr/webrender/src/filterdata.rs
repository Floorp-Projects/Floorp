/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{hash};
use gpu_cache::{GpuCacheHandle};
use frame_builder::FrameBuildingState;
use gpu_cache::GpuDataRequest;
use intern;
use api::{ComponentTransferFuncType};


pub use intern_types::filterdata::Handle as FilterDataHandle;

#[derive(Debug, Clone, MallocSizeOf, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum SFilterDataComponent {
    Identity,
    Table(Vec<f32>),
    Discrete(Vec<f32>),
    Linear(f32, f32),
    Gamma(f32, f32, f32),
}

impl Eq for SFilterDataComponent {}

impl hash::Hash for SFilterDataComponent {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        match self {
            SFilterDataComponent::Identity => {
                0.hash(state);
            }
            SFilterDataComponent::Table(values) => {
                1.hash(state);
                values.len().hash(state);
                for val in values {
                    val.to_bits().hash(state);
                }
            }
            SFilterDataComponent::Discrete(values) => {
                2.hash(state);
                values.len().hash(state);
                for val in values {
                    val.to_bits().hash(state);
                }
            }
            SFilterDataComponent::Linear(a, b) => {
                3.hash(state);
                a.to_bits().hash(state);
                b.to_bits().hash(state);
            }
            SFilterDataComponent::Gamma(a, b, c) => {
                4.hash(state);
                a.to_bits().hash(state);
                b.to_bits().hash(state);
                c.to_bits().hash(state);
            }
        }
    }
}

#[derive(Debug, Clone, MallocSizeOf, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SFilterData {
    pub r_func: SFilterDataComponent,
    pub g_func: SFilterDataComponent,
    pub b_func: SFilterDataComponent,
    pub a_func: SFilterDataComponent,
}

#[derive(Debug, Clone, MallocSizeOf, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SFilterDataKey {
    pub data: SFilterData,
}

impl intern::InternDebug for SFilterDataKey {}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct SFilterDataTemplate {
    pub data: SFilterData,
    pub gpu_cache_handle: GpuCacheHandle,
}

impl From<SFilterDataKey> for SFilterDataTemplate {
    fn from(item: SFilterDataKey) -> Self {
        SFilterDataTemplate {
            data: item.data,
            gpu_cache_handle: GpuCacheHandle::new(),
        }
    }
}
