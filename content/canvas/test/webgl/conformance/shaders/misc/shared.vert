// shared vertex shader should succeed.
uniform mat4 viewProjection;
uniform vec3 worldPosition;
uniform vec3 nextPosition;
uniform float fishLength;
uniform float fishWaveLength;
uniform float fishBendAmount;
attribute vec4 position;
attribute vec2 texCoord;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_surfaceToLight;
void main() {
  vec3 vz = normalize(worldPosition - nextPosition);
  vec3 vx = normalize(cross(vec3(0,1,0), vz));
  vec3 vy = cross(vz, vx);
  mat4 orientMat = mat4(
    vec4(vx, 0),
    vec4(vy, 0),
    vec4(vz, 0),
    vec4(worldPosition, 1));
  mat4 world = orientMat;
  mat4 worldViewProjection = viewProjection * world;
  mat4 worldInverseTranspose = world;

  v_texCoord = texCoord;
  // NOTE:If you change this you need to change the laser code to match!
  float mult = position.z > 0.0 ?
      (position.z / fishLength) :
      (-position.z / fishLength * 2.0);
  float s = sin(mult * fishWaveLength);
  float a = sign(s);
  float offset = pow(mult, 2.0) * s * fishBendAmount;
  v_position = (
      worldViewProjection *
      (position +
       vec4(offset, 0, 0, 0)));
  v_surfaceToLight = (world * position).xyz;
  gl_Position = v_position;
}

