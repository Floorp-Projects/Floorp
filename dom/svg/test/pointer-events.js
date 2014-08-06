SimpleTest.waitForExplicitFinish();

var pointer_events_values = [
  'auto',
  'visiblePainted',
  'visibleFill',
  'visibleStroke',
  'visible',
  'painted',
  'fill',
  'stroke',
  'all',
  'none'
];

var paint_values = [
  'blue',
  'transparent',
  'none'
];

var opacity_values = [
  '1',
  '0.5',
  '0'
];

var visibility_values = [
  'visible',
  'hidden',
  'collapse'
];

/**
 * List of attributes and various values for which we want to test permutations
 * when hit testing a pointer event that is over an element's fill area,
 * stroke area, or both (where they overlap).
 *
 * We're using an array of objects so that we have control over the order in
 * which permutations are tested.
 *
 * TODO: test the effect of clipping, masking, filters, markers, etc.
 */
var hit_test_inputs = {
  fill: [
    { name: 'pointer-events',  values: pointer_events_values },
    { name: 'fill',            values: paint_values },
    { name: 'fill-opacity',    values: opacity_values },
    { name: 'opacity',         values: opacity_values },
    { name: 'visibility',      values: visibility_values }
  ],
  stroke: [
    { name: 'pointer-events',  values: pointer_events_values },
    { name: 'stroke',          values: paint_values },
    { name: 'stroke-opacity',  values: opacity_values },
    { name: 'opacity',         values: opacity_values },
    { name: 'visibility',      values: visibility_values }
  ],
  both: [
    { name: 'pointer-events',  values: pointer_events_values },
    { name: 'fill',            values: paint_values },
    { name: 'fill-opacity',    values: opacity_values },
    { name: 'stroke',          values: paint_values },
    { name: 'stroke-opacity',  values: opacity_values },
    { name: 'opacity',         values: opacity_values },
    { name: 'visibility',      values: visibility_values }
  ]
}

/**
 * The following object contains a list of 'pointer-events' property values,
 * each with an object detailing the conditions under which the fill and stroke
 * of a graphical object will intercept pointer events for the given value. If
 * the object contains a 'fill-intercepts-iff' property then the fill is
 * expected to intercept pointer events for that value of 'pointer-events' if
 * and only if the conditions listed in the 'fill-intercepts-iff' object are
 * met. If there are no conditions in the 'fill-intercepts-iff' object then the
 * fill should always intercept pointer events. However, if the
 * 'fill-intercepts-iff' property is not set at all then it indicates that the
 * fill should never intercept pointer events. The same rules apply for
 * 'stroke-intercepts-iff'.
 *
 * If an attribute name in the conditions list is followed by the "!"
 * character then the requirement for a hit is that its value is NOT any
 * of the values listed in the given array.
 */
var hit_conditions = {
  auto: {
    'fill-intercepts-iff': {
      'visibility': ['visible'],
      'fill!': ['none']
    },
    'stroke-intercepts-iff': {
      'visibility': ['visible'],
      'stroke!': ['none']
    }
  },
  visiblePainted: {
    'fill-intercepts-iff': {
      'visibility': ['visible'],
      'fill!': ['none']
    },
    'stroke-intercepts-iff': {
      'visibility': ['visible'],
      'stroke!': ['none']
    }
  },
  visibleFill: {
    'fill-intercepts-iff': {
      visibility: ['visible']
    }
    // stroke never intercepts pointer events
  },
  visibleStroke: {
    // fill never intercepts pointer events
    'stroke-intercepts-iff': {
      visibility: ['visible']
    }
  },
  visible: {
    'fill-intercepts-iff': {
      visibility: ['visible']
    },
    'stroke-intercepts-iff': {
      visibility: ['visible']
    }
  },
  painted: {
    'fill-intercepts-iff': {
      'fill!': ['none']
    },
    'stroke-intercepts-iff': {
      'stroke!': ['none']
    }
  },
  fill: {
    'fill-intercepts-iff': {
      // fill always intercepts pointer events
    }
    // stroke never intercepts pointer events
  },
  stroke: {
    // fill never intercepts pointer events
    'stroke-intercepts-iff': {
      // stroke always intercepts pointer events
    }
  },
  all: {
    'fill-intercepts-iff': {
      // fill always intercepts pointer events
    },
    'stroke-intercepts-iff': {
      // stroke always intercepts pointer events
    }
  },
  none: {
    // neither fill nor stroke intercept pointer events
  }
}

// bit flags
var POINT_OVER_FILL   = 0x1;
var POINT_OVER_STROKE = 0x2;

/**
 * Examine the element's attribute values and, based on the area(s) of the
 * element that the pointer event is over (fill and/or stroke areas), return
 * true if the element is expected to intercept the event, otherwise false.
 */
function hit_expected(element, over /* bit flags indicating which area(s) of the element the pointer is over */)
{
  function expect_hit(target){
    var intercepts_iff =
      hit_conditions[element.getAttribute('pointer-events')][target + '-intercepts-iff'];

    if (!intercepts_iff) {
      return false; // never intercepts events
    }

    for (var attr in intercepts_iff) {
      var vals = intercepts_iff[attr];  // must get this before we adjust 'attr'
      var invert = false;
      if (attr.substr(-1) == '!') {
        invert = true;
        attr = attr.substr(0, attr.length-1);
      }
      var match = vals.indexOf(element.getAttribute(attr)) > -1;
      if (invert) {
        match = !match;
      }
      if (!match) {
        return false;
      }
    }

    return true;
  }

  return (over & POINT_OVER_FILL) != 0   && expect_hit('fill') ||
         (over & POINT_OVER_STROKE) != 0 && expect_hit('stroke');
}

function for_all_permutations(inputs, callback)
{
  var current_permutation = arguments[2] || {};
  var index = arguments[3] || 0;

  if (index < inputs.length) {
    var name = inputs[index].name;
    var values = inputs[index].values;
    for (var i = 0; i < values.length; ++i) {
      current_permutation[name] = values[i];
      for_all_permutations(inputs, callback, current_permutation, index+1);
    }
    return;
  }

  callback(current_permutation);
}

function make_log_msg(over, tag, attributes)
{
  var target;
  if (over == (POINT_OVER_FILL | POINT_OVER_STROKE)) {
    target = 'fill and stroke';
  } else if (over == POINT_OVER_FILL) {
    target = 'fill';
  } else if (over == POINT_OVER_STROKE) {
    target = 'stroke';
  } else {
    throw "unexpected bit combination in 'over'";
  }
  var msg = 'Check if events are intercepted at a point over the '+target+' on <'+tag+'> for';
  for (var attr in attributes) {
    msg += ' '+attr+'='+attributes[attr];
  }
  return msg;
}

var dx, dy; // offset of <svg> element from pointer coordinates origin

function test_element(id, x, y, over /* bit flags indicating which area(s) of the element the pointer is over */)
{
  var element = document.getElementById(id);
  var tag = element.tagName;

  function test_permutation(attributes) {
    for (var attr in attributes) {
      element.setAttribute(attr, attributes[attr]);
    }
    var hits = document.elementFromPoint(dx + x, dy + y) == element;
    var msg = make_log_msg(over, tag, attributes);

    is(hits, hit_expected(element, over), msg);
  }

  var inputs;
  if (over == (POINT_OVER_FILL | POINT_OVER_STROKE)) {
    inputs = hit_test_inputs['both'];
  } else if (over == POINT_OVER_FILL) {
    inputs = hit_test_inputs['fill'];
  } else if (over == POINT_OVER_STROKE) {
    inputs = hit_test_inputs['stroke'];
  } else {
    throw "unexpected bit combination in 'over'";
  }

  for_all_permutations(inputs, test_permutation);

  // To reduce the chance of bogus results in subsequent tests:
  element.setAttribute('fill', 'none');
  element.setAttribute('stroke', 'none');
}

function run_tests(subtest)
{
  var div = document.getElementById("div");
  dx = div.offsetLeft;
  dy = div.offsetTop;

  // Run the test with only a subset of pointer-events values, to avoid
  // running over the mochitest time limit.  The subtest argument indicates
  // whether to use the first half of the pointer-events values (0)
  // or the second half (1).
  var partition = Math.floor(pointer_events_values.length / 2);
  switch (subtest) {
    case 0:
      pointer_events_values.splice(partition);
      break;
    case 1:
      pointer_events_values.splice(0, partition);
      break;
    case 2:
      throw "unexpected subtest number";
  }

  test_element('rect', 30, 30, POINT_OVER_FILL);
  test_element('rect', 5, 5, POINT_OVER_STROKE);

  // The SVG 1.1 spec essentially says that, for text, hit testing is done
  // against the character cells of the text, and not the fill and stroke as
  // you might expect for a normal graphics element like <path>. See the
  // paragraph starting "For text elements..." in this section:
  //
  //   http://www.w3.org/TR/SVG11/interact.html#PointerEventsProperty
  //
  // This requirement essentially means that for the purposes of hit testing
  // the fill and stroke areas are the same area - the character cell. (At
  // least until we support having any fill or stroke that lies outside the
  // character cells intercept events like Opera does - see below.) Thus, for
  // text, when a pointer event is over a character cell it is essentially over
  // both the fill and stroke at the same time. That's the reason we pass both
  // the POINT_OVER_FILL and POINT_OVER_STROKE bits in test_element's 'over'
  // argument below. It's also the reason why we only test one point in the
  // text rather than having separate tests for fill and stroke.
  //
  // For hit testing of text, Opera essentially treats fill and stroke like it
  // would on any normal element, but it adds the character cells of glyhs to
  // both the glyphs' fill AND stroke. I think this is what we should do too.
  // It's compatible with the letter of the SVG 1.1 rules, and it allows any
  // parts of a glyph that are outside the glyph's character cells to also
  // intercept events in the normal way. When we make that change we'll be able
  // to add separate fill and stroke tests for text below.

  test_element('text', 210, 30, POINT_OVER_FILL | POINT_OVER_STROKE);

  SimpleTest.finish();
}
