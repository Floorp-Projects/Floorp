precision mediump float;

uniform vec4 uColor;
varying vec2 vTextureCoordinate;

void main() {
  gl_FragColor = uColor;
  gl_FragColor = vec4(vTextureCoordinate.x, vTextureCoordinate.y, 0, 0.5);

  float u = vTextureCoordinate.x;
  float v = vTextureCoordinate.y;
  float r = u * u - v;
  if (r < 0.0) {
    gl_FragColor = vec4(1, 0, 0, 1);
  } else {
    gl_FragColor = vec4(1, 0, 0, 0.2);
  }
}