# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

tempfile=tmpShaderHeader
rm CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ELayerQuadVS -nologo -Tvs_4_0_level_9_3 -Fh$tempfile -VnLayerQuadVS
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ESolidColorShader -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnSolidColorShader
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ERGBShader -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnRGBShader
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ERGBAShader -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnRGBAShader
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -EComponentAlphaShader -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnComponentAlphaShader
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -EYCbCrShader -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnYCbCrShader
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ELayerQuadMaskVS -nologo -Tvs_4_0_level_9_3 -Fh$tempfile -VnLayerQuadMaskVS
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ELayerQuadMask3DVS -nologo -Tvs_4_0_level_9_3 -Fh$tempfile -VnLayerQuadMask3DVS
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ESolidColorShaderMask -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnSolidColorShaderMask
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ERGBShaderMask -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnRGBShaderMask
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ERGBAShaderMask -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnRGBAShaderMask
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -ERGBAShaderMask3D -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnRGBAShaderMask3D
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -EYCbCrShaderMask -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnYCbCrShaderMask
cat $tempfile >> CompositorD3D11Shaders.h
fxc CompositorD3D11.hlsl -EComponentAlphaShaderMask -Tps_4_0_level_9_3 -nologo -Fh$tempfile -VnComponentAlphaShaderMask
cat $tempfile >> CompositorD3D11Shaders.h
rm $tempfile
