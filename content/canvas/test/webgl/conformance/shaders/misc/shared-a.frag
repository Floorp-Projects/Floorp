// shared fragment shader should succeed.
precision mediump float;
uniform vec4 lightColor;
varying vec4 v_position;
varying vec2 v_texCoord;
varying vec3 v_surfaceToLight;

uniform vec4 ambient;
uniform sampler2D diffuse;
uniform vec4 specular;
uniform float shininess;
uniform float specularFactor;
// #fogUniforms

vec4 lit(float l ,float h, float m) {
  return vec4(1.0,
              max(l, 0.0),
              (l > 0.0) ? pow(max(0.0, h), m) : 0.0,
              1.0);
}
void main() {
  vec4 diffuseColor = texture2D(diffuse, v_texCoord);
  vec4 normalSpec = vec4(0,0,0,0);  // #noNormalMap
  vec3 surfaceToLight = normalize(v_surfaceToLight);
  vec3 halfVector = normalize(surfaceToLight);
  vec4 litR = lit(1.0, 1.0, shininess);
  vec4 outColor = vec4(
    (lightColor * (diffuseColor * litR.y + diffuseColor * ambient +
                  specular * litR.z * specularFactor * normalSpec.a)).rgb,
      diffuseColor.a);
  // #fogCode
  gl_FragColor = outColor;
}

