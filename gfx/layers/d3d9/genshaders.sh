# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

tempfile=tmpShaderHeader
rm LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ELayerQuadVS -nologo -Fh$tempfile -VnLayerQuadVS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ERGBAShader -nologo -Tps_2_0 -Fh$tempfile -VnRGBAShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -EComponentPass1Shader -nologo -Tps_2_0 -Fh$tempfile -VnComponentPass1ShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -EComponentPass2Shader -nologo -Tps_2_0 -Fh$tempfile -VnComponentPass2ShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ERGBShader -nologo -Tps_2_0 -Fh$tempfile -VnRGBShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -EYCbCrShader -nologo -Tps_2_0 -Fh$tempfile -VnYCbCrShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ESolidColorShader -nologo -Tps_2_0 -Fh$tempfile -VnSolidColorShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ELayerQuadVSMask -nologo -Fh$tempfile -VnLayerQuadVSMask
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ERGBAShaderMask -nologo -Tps_2_0 -Fh$tempfile -VnRGBAShaderPSMask
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -EComponentPass1ShaderMask -nologo -Tps_2_0 -Fh$tempfile -VnComponentPass1ShaderPSMask
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -EComponentPass2ShaderMask -nologo -Tps_2_0 -Fh$tempfile -VnComponentPass2ShaderPSMask
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ERGBShaderMask -nologo -Tps_2_0 -Fh$tempfile -VnRGBShaderPSMask
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -EYCbCrShaderMask -nologo -Tps_2_0 -Fh$tempfile -VnYCbCrShaderPSMask
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ESolidColorShaderMask -nologo -Tps_2_0 -Fh$tempfile -VnSolidColorShaderPSMask
cat $tempfile >> LayerManagerD3D9Shaders.h
rm $tempfile
