precision mediump float;

varying vec4 vColor;
uniform mat4 uColorMatrix;
uniform vec4 uColorVector;
uniform sampler2D uSampler[8];
varying vec2 vCoordinate;
varying float vKind;
varying float vSampler;

void main() {
  vec4 color;
  int kind = int(floor(vKind + 0.5));
  if (kind == 0) {
    color = vColor;
  } else if (kind == 1 || kind == 2) {
    int sampler = int(floor(vSampler + 0.5));
    if (sampler == 0) {
      color = vColor * texture2D(uSampler[0], vCoordinate);
    } else if (sampler == 1) {
      color = vColor * texture2D(uSampler[1], vCoordinate);
    } else if (sampler == 2) {
      color = vColor * texture2D(uSampler[2], vCoordinate);
    } else if (sampler == 3) {
      color = vColor * texture2D(uSampler[3], vCoordinate);
    } else if (sampler == 4) {
      color = vColor * texture2D(uSampler[4], vCoordinate);
    } else if (sampler == 5) {
      color = vColor * texture2D(uSampler[5], vCoordinate);
    } else if (sampler == 6) {
      color = vColor * texture2D(uSampler[6], vCoordinate);
    } else if (sampler == 7) {
      color = vColor * texture2D(uSampler[7], vCoordinate);
    }
    if (kind == 2) {
      color = color * uColorMatrix + uColorVector;
    }
  } else {
    color = vec4(1.0, 0.0, 0.0, 1.0);
  }
  // color.rgb *= color.a;
  if (color.a < 0.01) {
    discard;
  }
  gl_FragColor = color;
}