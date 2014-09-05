uniform vec2 uResolution;
uniform mat3 uTransformMatrix;
uniform mat4 uTransformMatrix3D;

attribute vec4 aPosition;
attribute vec4 aColor;
attribute vec2 aCoordinate;
attribute float aKind;
attribute float aSampler;

varying vec4 vColor;
varying vec2 vCoordinate;
varying float vKind;
varying float vSampler;

void main() {
  gl_Position = uTransformMatrix3D * aPosition;
  vColor = aColor;
  vCoordinate = aCoordinate;
  vKind = aKind;
  vSampler = aSampler;
}