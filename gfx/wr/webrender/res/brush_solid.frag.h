ALWAYS_INLINE int draw_span(uint32_t* buf, int len) {
  auto color = pack_span(buf, flat_varying_vec4_0);
  commit_solid_span(buf, color, len);
  return len;
}

ALWAYS_INLINE int draw_span(uint8_t* buf, int len) {
  auto color = pack_span(buf, flat_varying_vec4_0.x);
  commit_solid_span(buf, color, len);
  return len;
}
