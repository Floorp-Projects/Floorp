#line 1

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void discard_pixels_in_rounded_borders(vec2 local_pos) {
  float distanceFromRef = distance(vRefPoint, local_pos);
  if (vRadii.x > 0.0 && (distanceFromRef > vRadii.x || distanceFromRef < vRadii.z)) {
      discard;
  }
}

vec4 get_fragment_color(float distanceFromMixLine, float pixelsPerFragment) {
  // Here we are mixing between the two border colors. We need to convert
  // distanceFromMixLine it to pixel space to properly anti-alias and then push
  // it between the limits accepted by `mix`.
  float colorMix = min(max(distanceFromMixLine / pixelsPerFragment, -0.5), 0.5) + 0.5;
  return mix(vHorizontalColor, vVerticalColor, colorMix);
}

float alpha_for_solid_border(float distance_from_ref,
                             float inner_radius,
                             float outer_radius,
                             float pixels_per_fragment) {
  // We want to start anti-aliasing one pixel in from the border.
  float nudge = pixels_per_fragment;
  inner_radius += nudge;
  outer_radius -= nudge;

  if (distance_from_ref < outer_radius && distance_from_ref > inner_radius) {
    return 1.0;
  }

  float distance_from_border = max(distance_from_ref - outer_radius,
                                   inner_radius - distance_from_ref);

  // Move the distance back into pixels.
  distance_from_border /= pixels_per_fragment;

  // Apply a more gradual fade out to transparent.
  // distance_from_border -= 0.5;

  return 1.0 - smoothstep(0.0, 1.0, distance_from_border);
}

float alpha_for_solid_ellipse_border(vec2 local_pos,
                                     vec2 inner_radius,
                                     vec2 outer_radius,
                                     float pixels_per_fragment) {
  vec2 distance_from_ref = local_pos - vRefPoint;

  float nudge = pixels_per_fragment;
  inner_radius += nudge;
  outer_radius -= nudge;

  float inner_ellipse = distance_from_ref.x * distance_from_ref.x / inner_radius.x / inner_radius.x +
                        distance_from_ref.y * distance_from_ref.y / inner_radius.y / inner_radius.y;
  float outer_ellipse = distance_from_ref.x * distance_from_ref.x / outer_radius.x / outer_radius.x +
                        distance_from_ref.y * distance_from_ref.y / outer_radius.y / outer_radius.y;
  if (inner_ellipse > 1.0 && outer_ellipse < 1.0) {
      return 1.0;
  }

  vec2 offset = step(inner_radius.yx, inner_radius.xy) *
                (sqrt(abs(inner_radius.x * inner_radius.x - inner_radius.y * inner_radius.y)));
  vec2 focus1 = vRefPoint + offset;
  vec2 focus2 = vRefPoint - offset;

  float inner_distance_from_border = max(inner_radius.x, inner_radius.y) -
                                     (distance(focus1, local_pos) + distance(focus2, local_pos)) / 2.0;

  offset = step(outer_radius.yx, outer_radius.xy) *
           (sqrt(abs(outer_radius.x * outer_radius.x - outer_radius.y * outer_radius.y)));
  focus1 = vRefPoint + offset;
  focus2 = vRefPoint - offset;
  float outer_distance_from_border = (distance(focus1, local_pos) + distance(focus2, local_pos)) / 2.0 -
                                     max(outer_radius.x, outer_radius.y);

  float distance_from_border = max(inner_distance_from_border, outer_distance_from_border);

  // Move the distance back into pixels.
  distance_from_border /= pixels_per_fragment;

  return 1.0 - smoothstep(0.0, 1.0, distance_from_border);
}

float alpha_for_solid_border_corner(vec2 local_pos,
                                    vec2 inner_radius,
                                    vec2 outer_radius,
                                    float pixels_per_fragment) {
  if (inner_radius.x == inner_radius.y && outer_radius.x == outer_radius.y) {
    float distance_from_ref = distance(vRefPoint, local_pos);
    return alpha_for_solid_border(distance_from_ref, inner_radius.x, outer_radius.x, pixels_per_fragment);
  } else {
    return alpha_for_solid_ellipse_border(local_pos, inner_radius, outer_radius, pixels_per_fragment);
  }
}

vec4 draw_dotted_edge(vec2 local_pos, vec4 piece_rect, float pixels_per_fragment) {
  // We don't use pixels_per_fragment here, since it can change along the edge
  // of a transformed border edge. We want this calculation to be consistent
  // across the entire edge so that the positioning of the dots stays the same.
  float two_pixels = 2.0 * length(fwidth(vLocalPos.xy));

  // Circle diameter is stroke width, minus a couple pixels to account for anti-aliasing.
  float circle_diameter = max(piece_rect.z - two_pixels, min(piece_rect.z, two_pixels));

  // We want to spread the circles across the edge, but keep one circle diameter at the end
  // reserved for two half-circles which connect to the corners.
  float edge_available = piece_rect.w - (circle_diameter * 2.0);
  float number_of_circles = floor(edge_available / (circle_diameter * 2.0));

  // Here we are initializing the distance from the y coordinate of the center of the circle to
  // the closest end half-circle.
  vec2 relative_pos = local_pos - piece_rect.xy;
  float y_distance = min(relative_pos.y, piece_rect.w - relative_pos.y);

  if (number_of_circles > 0.0) {
    // Spread the circles throughout the edge, to distribute the extra space evenly. We want
    // to ensure that we have at last two pixels of space for each circle so that they aren't
    // touching.
    float space_for_each_circle = ceil(max(edge_available / number_of_circles, two_pixels));

    float first_half_circle_space = circle_diameter;

    float circle_index = (relative_pos.y - first_half_circle_space) / space_for_each_circle;
    circle_index = floor(clamp(circle_index, 0.0, number_of_circles - 1.0));

    float circle_y_pos =
      circle_index * space_for_each_circle + (space_for_each_circle / 2.0) + circle_diameter;
    y_distance = min(abs(circle_y_pos - relative_pos.y), y_distance);
  }

  float distance_from_circle_center = length(vec2(relative_pos.x - (piece_rect.z / 2.0), y_distance));
  float distance_from_circle_edge = distance_from_circle_center - (circle_diameter / 2.0);

  // Don't anti-alias if the circle diameter is small to avoid a blur of color.
  if (circle_diameter < two_pixels && distance_from_circle_edge > 0.0)
    return vec4(0.0);

  // Move the distance back into pixels.
  distance_from_circle_edge /= pixels_per_fragment;

  float alpha = 1.0 - smoothstep(0.0, 1.0, min(1.0, max(0.0, distance_from_circle_edge)));
  return vHorizontalColor * vec4(1.0, 1.0, 1.0, alpha);
}

vec4 draw_dashed_edge(float position, float border_width, float pixels_per_fragment) {
  // TODO: Investigate exactly what FF does.
  float size = border_width * 3.0;
  float segment = floor(position / size);

  float alpha = alpha_for_solid_border(position,
                                       segment * size,
                                       (segment + 1.0) * size,
                                       pixels_per_fragment);

  if (mod(segment + 2.0, 2.0) == 0.0) {
    return vHorizontalColor * vec4(1.0, 1.0, 1.0, 1.0 - alpha);
  } else {
    return vHorizontalColor * vec4(1.0, 1.0, 1.0, alpha);
  }
}

vec4 draw_dashed_or_dotted_border(vec2 local_pos, float distance_from_mix_line) {
  // This is the conversion factor for transformations and device pixel scaling.
  float pixels_per_fragment = length(fwidth(local_pos.xy));

  switch (vBorderPart) {
    // These are the layer tile part PrimitivePart as uploaded by the tiling.rs
    case PST_TOP_LEFT:
    case PST_TOP_RIGHT:
    case PST_BOTTOM_LEFT:
    case PST_BOTTOM_RIGHT:
    {
      vec4 color = get_fragment_color(distance_from_mix_line, pixels_per_fragment);
      if (vRadii.x > 0.0) {
        color.a *= alpha_for_solid_border_corner(local_pos,
                                                 vRadii.zw,
                                                 vRadii.xy,
                                                 pixels_per_fragment);
      }
      return color;
    }
    case PST_BOTTOM:
    case PST_TOP: {
      return vBorderStyle == BORDER_STYLE_DASHED ?
        draw_dashed_edge(vLocalPos.x - vPieceRect.x, vPieceRect.w, pixels_per_fragment) :
        draw_dotted_edge(local_pos.yx, vPieceRect.yxwz, pixels_per_fragment);
    }
    case PST_LEFT:
    case PST_RIGHT:
    {
      return vBorderStyle == BORDER_STYLE_DASHED ?
        draw_dashed_edge(vLocalPos.y - vPieceRect.y, vPieceRect.z, pixels_per_fragment) :
        draw_dotted_edge(local_pos.xy, vPieceRect.xyzw, pixels_per_fragment);
    }
  }

  return vec4(0.0);
}

vec4 draw_double_edge(float pos,
                      float len,
                      float distance_from_mix_line,
                      float pixels_per_fragment) {
  float total_border_width = len;
  float one_third_width = total_border_width / 3.0;

  // Contribution of the outer border segment.
  float alpha = alpha_for_solid_border(pos,
                                       total_border_width - one_third_width,
                                       total_border_width,
                                       pixels_per_fragment);

  // Contribution of the inner border segment.
  alpha += alpha_for_solid_border(pos, 0.0, one_third_width, pixels_per_fragment);
  return get_fragment_color(distance_from_mix_line, pixels_per_fragment) * vec4(1.0, 1.0, 1.0, alpha);
}

vec4 draw_double_edge_vertical(vec2 local_pos,
                               float distance_from_mix_line,
                               float pixels_per_fragment) {
  // Get our position within this specific segment
  float position = abs(local_pos.x - vRefPoint.x);
  return draw_double_edge(position, abs(vPieceRect.z), distance_from_mix_line, pixels_per_fragment);
}

vec4 draw_double_edge_horizontal(vec2 local_pos,
                                 float distance_from_mix_line,
                                 float pixels_per_fragment) {
  // Get our position within this specific segment
  float position = abs(local_pos.y - vRefPoint.y);
  return draw_double_edge(position, abs(vPieceRect.w), distance_from_mix_line, pixels_per_fragment);
}

vec4 draw_double_edge_corner_with_radius(vec2 local_pos,
                                         float distance_from_mix_line,
                                         float pixels_per_fragment) {
  float total_border_width = vRadii.x - vRadii.z;
  float one_third_width = total_border_width / 3.0;
  float total_border_height = vRadii.y - vRadii.w;
  float one_third_height = total_border_height / 3.0;

  // Contribution of the outer border segment.
  float alpha = alpha_for_solid_border_corner(local_pos,
                                              vec2(vRadii.x - one_third_width,
                                                   vRadii.y - one_third_height),
                                              vec2(vRadii.x, vRadii.y),
                                              pixels_per_fragment);

  // Contribution of the inner border segment.
  alpha += alpha_for_solid_border_corner(local_pos,
                                         vec2(vRadii.z, vRadii.w),
                                         vec2(vRadii.z + one_third_width, vRadii.w + one_third_height),
                                         pixels_per_fragment);
  return get_fragment_color(distance_from_mix_line, pixels_per_fragment) * vec4(1.0, 1.0, 1.0, alpha);
}

vec4 draw_double_edge_corner(vec2 local_pos,
                             float distance_from_mix_line,
                             float pixels_per_fragment) {
  if (vRadii.x > 0.0) {
      return draw_double_edge_corner_with_radius(local_pos,
                                                 distance_from_mix_line,
                                                 pixels_per_fragment);
  }

  bool is_vertical = (vBorderPart == PST_TOP_LEFT) ? distance_from_mix_line < 0.0 :
                                                     distance_from_mix_line >= 0.0;
  if (is_vertical) {
    return draw_double_edge_vertical(local_pos, distance_from_mix_line, pixels_per_fragment);
  } else {
    return draw_double_edge_horizontal(local_pos, distance_from_mix_line, pixels_per_fragment);
  }
}

vec4 draw_double_border(float distance_from_mix_line, vec2 local_pos) {
  float pixels_per_fragment = length(fwidth(local_pos.xy));
  switch (vBorderPart) {
    // These are the layer tile part PrimitivePart as uploaded by the tiling.rs
    case PST_TOP_LEFT:
    case PST_TOP_RIGHT:
    case PST_BOTTOM_LEFT:
    case PST_BOTTOM_RIGHT:
      return draw_double_edge_corner(local_pos, distance_from_mix_line, pixels_per_fragment);
    case PST_BOTTOM:
    case PST_TOP:
      return draw_double_edge_horizontal(local_pos,
                                         distance_from_mix_line,
                                         pixels_per_fragment);
    case PST_LEFT:
    case PST_RIGHT:
      return draw_double_edge_vertical(local_pos,
                                       distance_from_mix_line,
                                       pixels_per_fragment);
  }
  return vec4(0.0);
}

vec4 draw_solid_border(float distanceFromMixLine, vec2 localPos) {
  switch (vBorderPart) {
    case PST_TOP_LEFT:
    case PST_TOP_RIGHT:
    case PST_BOTTOM_LEFT:
    case PST_BOTTOM_RIGHT: {
      // This is the conversion factor for transformations and device pixel scaling.
      float pixelsPerFragment = length(fwidth(localPos.xy));
      vec4 color = get_fragment_color(distanceFromMixLine, pixelsPerFragment);

      if (vRadii.x > 0.0) {
        color.a *= alpha_for_solid_border_corner(localPos, vRadii.zw, vRadii.xy, pixelsPerFragment);
      }

      return color;
    }
    default:
      discard_pixels_in_rounded_borders(localPos);
      return vHorizontalColor;
  }
}

vec4 draw_mixed_edge(float distance, float border_len, vec4 color, vec2 brightness_mod) {
  float modulator = distance / border_len > 0.5 ? brightness_mod.x : brightness_mod.y;
  return vec4(color.xyz * modulator, color.a);
}

vec4 draw_mixed_border(float distanceFromMixLine, float distanceFromMiddle, vec2 localPos, vec2 brightness_mod) {
  switch (vBorderPart) {
    case PST_TOP_LEFT:
    case PST_TOP_RIGHT:
    case PST_BOTTOM_LEFT:
    case PST_BOTTOM_RIGHT: {
      // This is the conversion factor for transformations and device pixel scaling.
      float pixelsPerFragment = length(fwidth(localPos.xy));
      vec4 color = get_fragment_color(distanceFromMixLine, pixelsPerFragment);

      if (vRadii.x > 0.0) {
        float distance = distance(vRefPoint, localPos) - vRadii.z;
        float length = vRadii.x - vRadii.z;
        if (distanceFromMiddle < 0.0) {
          distance = length - distance;
        }

        return 0.0 <= distance && distance <= length ?
          draw_mixed_edge(distance, length, color, brightness_mod) :
          vec4(0.0, 0.0, 0.0, 0.0);
      }

      bool is_vertical = (vBorderPart == PST_TOP_LEFT) ? distanceFromMixLine < 0.0 :
                                                         distanceFromMixLine >= 0.0;
      float distance = is_vertical ? abs(localPos.x - vRefPoint.x) : abs(localPos.y - vRefPoint.y);
      float length = is_vertical ? abs(vPieceRect.z) : abs(vPieceRect.w);
      if (distanceFromMiddle > 0.0) {
          distance = length - distance;
      }

      return 0.0 <= distance && distance <= length ?
        draw_mixed_edge(distance, length, color, brightness_mod) :
        vec4(0.0, 0.0, 0.0, 0.0);
    }
    case PST_BOTTOM:
    case PST_TOP:
      return draw_mixed_edge(localPos.y - vPieceRect.y, vPieceRect.w, vVerticalColor, brightness_mod);
    case PST_LEFT:
    case PST_RIGHT:
      return draw_mixed_edge(localPos.x - vPieceRect.x, vPieceRect.z, vHorizontalColor, brightness_mod);
  }
  return vec4(0.0);
}

vec4 draw_complete_border(vec2 local_pos, float distance_from_mix_line, float distance_from_middle) {
  vec2 brightness_mod = vec2(0.7, 1.3);

  // Note: we can't pass-through in the following cases,
  // because Angle doesn't support it and fails to compile the shaders.
  switch (vBorderStyle) {
    case BORDER_STYLE_DASHED:
      return draw_dashed_or_dotted_border(local_pos, distance_from_mix_line);
    case BORDER_STYLE_DOTTED:
      return draw_dashed_or_dotted_border(local_pos, distance_from_mix_line);
    case BORDER_STYLE_DOUBLE:
      return draw_double_border(distance_from_mix_line, local_pos);
    case BORDER_STYLE_OUTSET:
      return draw_solid_border(distance_from_mix_line, local_pos);
    case BORDER_STYLE_INSET:
      return draw_solid_border(distance_from_mix_line, local_pos);
    case BORDER_STYLE_SOLID:
      return draw_solid_border(distance_from_mix_line, local_pos);
    case BORDER_STYLE_NONE:
      return draw_solid_border(distance_from_mix_line, local_pos);
    case BORDER_STYLE_GROOVE:
      return draw_mixed_border(distance_from_mix_line, distance_from_middle, local_pos, brightness_mod.yx);
    case BORDER_STYLE_RIDGE:
      return draw_mixed_border(distance_from_mix_line, distance_from_middle, local_pos, brightness_mod.xy);
    case BORDER_STYLE_HIDDEN:
    default:
      break;
  }

  // Note: Workaround for Angle on Windows,
  // because non-empty case statements must have break or return.
  discard;
  return vec4(0.0);
}

// TODO: Investigate performance of this shader and see
//       if it's worthwhile splitting it / removing branches etc.
void main(void) {
#ifdef WR_FEATURE_TRANSFORM
    float alpha = 0.0;
    vec2 local_pos = init_transform_fs(vLocalPos, vLocalRect, alpha);
#else
    float alpha = 1.0;
    vec2 local_pos = vLocalPos;
#endif

#ifdef WR_FEATURE_TRANSFORM
    // TODO(gw): Support other border styles for transformed elements.
    float distance_from_mix_line = (local_pos.x - vPieceRect.x) * vPieceRect.w -
                                   (local_pos.y - vPieceRect.y) * vPieceRect.z;
    distance_from_mix_line /= vPieceRectHypotenuseLength;
    float distance_from_middle = (local_pos.x - vBorderRect.p0.x) +
                                 (local_pos.y - vBorderRect.p0.y) -
                                 0.5 * (vBorderRect.size.x + vBorderRect.size.y);
#else
    float distance_from_mix_line = vDistanceFromMixLine;
    float distance_from_middle = vDistanceFromMiddle;
#endif

    oFragColor = draw_complete_border(local_pos, distance_from_mix_line, distance_from_middle);
    oFragColor.a *= min(alpha, do_clip());
}
