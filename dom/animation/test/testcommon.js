/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Use this variable if you specify duration or some other properties
 * for script animation.
 * E.g., div.animate({ opacity: [0, 1] }, 100 * MS_PER_SEC);
 *
 * NOTE: Creating animations with short duration may cause intermittent
 * failures in asynchronous test. For example, the short duration animation
 * might be finished when animation.ready has been fulfilled because of slow
 * platforms or busyness of the main thread.
 * Setting short duration to cancel its animation does not matter but
 * if you don't want to cancel the animation, consider using longer duration.
 */
const MS_PER_SEC = 1000;

/* The recommended minimum precision to use for time values[1].
 *
 * [1] https://w3c.github.io/web-animations/#precision-of-time-values
 */
var TIME_PRECISION = 0.0005; // ms

/*
 * Allow implementations to substitute an alternative method for comparing
 * times based on their precision requirements.
 */
function assert_times_equal(actual, expected, description) {
  assert_approx_equals(actual, expected, TIME_PRECISION, description);
}

/**
 * Appends a div to the document body and creates an animation on the div.
 * NOTE: This function asserts when trying to create animations with durations
 * shorter than 100s because the shorter duration may cause intermittent
 * failures.  If you are not sure how long it is suitable, use 100s; it's
 * long enough but shorter than our test framework timeout (330s).
 * If you really need to use shorter durations, use animate() function directly.
 *
 * @param t  The testharness.js Test object. If provided, this will be used
 *           to register a cleanup callback to remove the div when the test
 *           finishes.
 * @param attrs  A dictionary object with attribute names and values to set on
 *               the div.
 * @param frames  The keyframes passed to Element.animate().
 * @param options  The options passed to Element.animate().
 */
function addDivAndAnimate(t, attrs, frames, options) {
  let animDur = (typeof options === 'object') ?
    options.duration : options;
  assert_greater_than_equal(animDur, 100 * MS_PER_SEC,
      'Clients of this addDivAndAnimate API must request a duration ' +
      'of at least 100s, to avoid intermittent failures from e.g.' +
      'the main thread being busy for an extended period');

  return addDiv(t, attrs).animate(frames, options);
}

/**
 * Appends a div to the document body.
 *
 * @param t  The testharness.js Test object. If provided, this will be used
 *           to register a cleanup callback to remove the div when the test
 *           finishes.
 *
 * @param attrs  A dictionary object with attribute names and values to set on
 *               the div.
 */
function addDiv(t, attrs) {
  var div = document.createElement('div');
  if (attrs) {
    for (var attrName in attrs) {
      div.setAttribute(attrName, attrs[attrName]);
    }
  }
  document.body.appendChild(div);
  if (t && typeof t.add_cleanup === 'function') {
    t.add_cleanup(function() {
      if (div.parentNode) {
        div.parentNode.removeChild(div);
      }
    });
  }
  return div;
}

/**
 * Appends a style div to the document head.
 *
 * @param t  The testharness.js Test object. If provided, this will be used
 *           to register a cleanup callback to remove the style element
 *           when the test finishes.
 *
 * @param rules  A dictionary object with selector names and rules to set on
 *               the style sheet.
 */
function addStyle(t, rules) {
  var extraStyle = document.createElement('style');
  document.head.appendChild(extraStyle);
  if (rules) {
    var sheet = extraStyle.sheet;
    for (var selector in rules) {
      sheet.insertRule(selector + '{' + rules[selector] + '}',
                       sheet.cssRules.length);
    }
  }

  if (t && typeof t.add_cleanup === 'function') {
    t.add_cleanup(function() {
      extraStyle.remove();
    });
  }
}

/**
 * Promise wrapper for requestAnimationFrame.
 */
function waitForFrame() {
  return new Promise(function(resolve, reject) {
    window.requestAnimationFrame(resolve);
  });
}

/**
 * Returns a Promise that is resolved after the given number of consecutive
 * animation frames have occured (using requestAnimationFrame callbacks).
 *
 * @param frameCount  The number of animation frames.
 * @param onFrame  An optional function to be processed in each animation frame.
 */
function waitForAnimationFrames(frameCount, onFrame) {
  return new Promise(function(resolve, reject) {
    function handleFrame() {
      if (--frameCount <= 0) {
        resolve();
      } else {
        if (onFrame && typeof onFrame === 'function') {
          onFrame();
        }
        window.requestAnimationFrame(handleFrame); // wait another frame
      }
    }
    window.requestAnimationFrame(handleFrame);
  });
}

/**
 * Wrapper that takes a sequence of N animations and returns:
 *
 *   Promise.all([animations[0].ready, animations[1].ready, ... animations[N-1].ready]);
 */
function waitForAllAnimations(animations) {
  return Promise.all(animations.map(function(animation) {
    return animation.ready;
  }));
}

/**
 * Flush the computed style for the given element. This is useful, for example,
 * when we are testing a transition and need the initial value of a property
 * to be computed so that when we synchronouslyet set it to a different value
 * we actually get a transition instead of that being the initial value.
 */
function flushComputedStyle(elem) {
  var cs = window.getComputedStyle(elem);
  cs.marginLeft;
}

if (opener) {
  for (var funcName of ["async_test", "assert_not_equals", "assert_equals",
                        "assert_approx_equals", "assert_less_than",
                        "assert_less_than_equal", "assert_greater_than",
                        "assert_between_inclusive",
                        "assert_true", "assert_false",
                        "assert_class_string", "assert_throws",
                        "assert_unreached", "promise_test", "test"]) {
    window[funcName] = opener[funcName].bind(opener);
  }

  window.EventWatcher = opener.EventWatcher;

  function done() {
    opener.add_completion_callback(function() {
      self.close();
    });
    opener.done();
  }
}
