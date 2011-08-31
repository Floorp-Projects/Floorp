// shared fragment shader should succeed.
precision mediump float;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_surfaceToLight;

// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 normalSpec = vec4(0,0,0,0);  // #noNormalMap
  vec4 reflection = vec4(0,0,0,0);  // #noReflection
  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec4 skyColor = vec4(0.5,0.5,1,1);  // #noReflection

  vec3 halfVector = normalize(surfaceToLight);
  vec4 litR = lit(1.0, 1.0, 10.0);
  vec4 outColor = vec4(mix(
      skyColor,
      vec4(1,2,3,4) * (litR.y + litR.z * normalSpec.a),
      1.0 - reflection.r).rgb,
      1.0);
  // #fogCode
  gl_FragColor = outColor;
}

