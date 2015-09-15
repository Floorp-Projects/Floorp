# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

tempfile=tmpShaderHeader

FXC_DEBUG_FLAGS="-Zi -Fd shaders.pdb"
FXC_FLAGS=""

# If DEBUG is in the environment, then rebuild with debug info
if [ "$DEBUG" != "" ] ; then
  FXC_FLAGS="$FXC_DEBUG_FLAGS"
fi

makeShaderVS() {
    fxc -nologo $FXC_FLAGS -Tvs_4_0_level_9_3 $SRC -E$1 -Vn$1 -Fh$tempfile
    echo "ShaderBytes s$1 = { $1, sizeof($1) };" >> $tempfile;
    cat $tempfile >> $DEST
}

makeShaderPS() {
    fxc -nologo $FXC_FLAGS -Tps_4_0_level_9_3 $SRC -E$1 -Vn$1 -Fh$tempfile
    echo "ShaderBytes s$1 = { $1, sizeof($1) };" >> $tempfile;
    cat $tempfile >> $DEST
}

SRC=CompositorD3D11.hlsl
DEST=CompositorD3D11Shaders.h

rm -f $DEST
echo "struct ShaderBytes { const void* mData; size_t mLength; };" >> $DEST;
makeShaderVS LayerQuadVS
makeShaderPS SolidColorShader
makeShaderPS RGBShader
makeShaderPS RGBAShader
makeShaderPS ComponentAlphaShader
makeShaderPS YCbCrShader
makeShaderVS LayerQuadMaskVS
makeShaderVS LayerQuadMask3DVS
makeShaderPS SolidColorShaderMask
makeShaderPS RGBShaderMask
makeShaderPS RGBAShaderMask
makeShaderPS RGBAShaderMask3D
makeShaderPS YCbCrShaderMask
makeShaderPS ComponentAlphaShaderMask

SRC=CompositorD3D11VR.hlsl
DEST=CompositorD3D11ShadersVR.h

rm -f $DEST
makeShaderVS Oculus050VRDistortionVS
makeShaderPS Oculus050VRDistortionPS

rm $tempfile
