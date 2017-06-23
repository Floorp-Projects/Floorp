float4 Blend{BLEND_FUNC}PS(const VS_BLEND_OUTPUT aInput) : SV_Target
{
  if (!RectContainsPoint(aInput.vClipRect, aInput.vPosition.xy)) {
    discard;
  }

  float4 backdrop = tBackdrop.Sample(sSampler, aInput.vBackdropCoords);
  float4 source = simpleTex.Sample(sSampler, aInput.vTexCoords);

  // Apply masks to the source texture, not the result.
  source *= ReadMask(aInput.vMaskCoords);

  // Shortcut when the backdrop or source alpha is 0, otherwise we may leak
  // infinity into the blend function and return incorrect results.
  if (backdrop.a == 0.0) {
    return source;
  }
  if (source.a == 0.0) {
    return float4(0, 0, 0, 0);
  }

  // The spec assumes there is no premultiplied alpha. The backdrop and
  // source are both render targets and always premultiplied, so we undo
  // that here.
  backdrop.rgb /= backdrop.a;
  source.rgb /= source.a;

  float4 result;
  result.rgb = Blend{BLEND_FUNC}(backdrop.rgb, source.rgb);
  result.a = source.a;

  // Factor backdrop alpha, then premultiply for the final OP_OVER.
  result.rgb = (1.0 - backdrop.a) * source.rgb + backdrop.a * result.rgb;
  result.rgb *= result.a;
  return result;
}
