tempfile=tmpShaderHeader
rm LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ELayerQuadVS -nologo -Fh$tempfile -VnLayerQuadVS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ERGBAShader -nologo -Tps_2_0 -Fh$tempfile -VnRGBAShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ERGBShader -nologo -Tps_2_0 -Fh$tempfile -VnRGBShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -EYCbCrShader -nologo -Tps_2_0 -Fh$tempfile -VnYCbCrShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
fxc LayerManagerD3D9Shaders.hlsl -ESolidColorShader -nologo -Tps_2_0 -Fh$tempfile -VnSolidColorShaderPS
cat $tempfile >> LayerManagerD3D9Shaders.h
rm $tempfile
