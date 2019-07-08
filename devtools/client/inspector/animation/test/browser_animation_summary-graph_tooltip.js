/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for existance and content of tooltip on summary graph element.

const TEST_DATA = [
  {
    targetClass: "cssanimation-normal",
    expectedResult: {
      nameAndType: "cssanimation - CSS Animation",
      duration: "1,000s",
    },
  },
  {
    targetClass: "cssanimation-linear",
    expectedResult: {
      nameAndType: "cssanimation - CSS Animation",
      duration: "1,000s",
      animationTimingFunction: "linear",
    },
  },
  {
    targetClass: "delay-positive",
    expectedResult: {
      nameAndType: "test-delay-animation - Script Animation",
      delay: "500s",
      duration: "1,000s",
    },
  },
  {
    targetClass: "delay-negative",
    expectedResult: {
      nameAndType: "test-negative-delay-animation - Script Animation",
      delay: "-500s",
      duration: "1,000s",
    },
  },
  {
    targetClass: "easing-step",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      easing: "steps(2)",
    },
  },
  {
    targetClass: "enddelay-positive",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      endDelay: "500s",
    },
  },
  {
    targetClass: "enddelay-negative",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      endDelay: "-500s",
    },
  },
  {
    targetClass: "enddelay-with-fill-forwards",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      endDelay: "500s",
      fill: "forwards",
    },
  },
  {
    targetClass: "enddelay-with-iterations-infinity",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      endDelay: "500s",
      iterations: "\u221E",
    },
  },
  {
    targetClass: "direction-alternate-with-iterations-infinity",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      direction: "alternate",
      iterations: "\u221E",
    },
  },
  {
    targetClass: "direction-alternate-reverse-with-iterations-infinity",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      direction: "alternate-reverse",
      iterations: "\u221E",
    },
  },
  {
    targetClass: "direction-reverse-with-iterations-infinity",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      direction: "reverse",
      iterations: "\u221E",
    },
  },
  {
    targetClass: "fill-backwards",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      fill: "backwards",
    },
  },
  {
    targetClass: "fill-backwards-with-delay-iterationstart",
    expectedResult: {
      nameAndType: "Script Animation",
      delay: "500s",
      duration: "1,000s",
      fill: "backwards",
      iterationStart: "0.5",
    },
  },
  {
    targetClass: "fill-both",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      fill: "both",
    },
  },
  {
    targetClass: "fill-both-width-delay-iterationstart",
    expectedResult: {
      nameAndType: "Script Animation",
      delay: "500s",
      duration: "1,000s",
      fill: "both",
      iterationStart: "0.5",
    },
  },
  {
    targetClass: "fill-forwards",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      fill: "forwards",
    },
  },
  {
    targetClass: "iterationstart",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
      iterationStart: "0.5",
    },
  },
  {
    targetClass: "no-compositor",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
    },
  },
  {
    targetClass: "keyframes-easing-step",
    expectedResult: {
      nameAndType: "Script Animation",
      duration: "1,000s",
    },
  },
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept(TEST_DATA.map(t => `.${t.targetClass}`));
  const { panel } = await openAnimationInspector();

  for (const { targetClass, expectedResult } of TEST_DATA) {
    const animationItemEl = findAnimationItemElementsByTargetSelector(
      panel,
      `.${targetClass}`
    );
    const summaryGraphEl = animationItemEl.querySelector(
      ".animation-summary-graph"
    );

    info(`Checking tooltip for ${targetClass}`);
    ok(
      summaryGraphEl.hasAttribute("title"),
      "Summary graph should have 'title' attribute"
    );

    const tooltip = summaryGraphEl.getAttribute("title");
    const {
      animationTimingFunction,
      delay,
      easing,
      endDelay,
      direction,
      duration,
      fill,
      iterations,
      iterationStart,
      nameAndType,
    } = expectedResult;

    ok(
      tooltip.startsWith(nameAndType),
      "Tooltip should start with name and type"
    );

    if (animationTimingFunction) {
      const expected = `Animation timing function: ${animationTimingFunction}`;
      ok(tooltip.includes(expected), `Tooltip should include '${expected}'`);
    } else {
      ok(
        !tooltip.includes("Animation timing function:"),
        "Tooltip should not include animation timing function"
      );
    }

    if (delay) {
      const expected = `Delay: ${delay}`;
      ok(tooltip.includes(expected), `Tooltip should include '${expected}'`);
    } else {
      ok(!tooltip.includes("Delay:"), "Tooltip should not include delay");
    }

    if (direction) {
      const expected = `Direction: ${direction}`;
      ok(tooltip.includes(expected), `Tooltip should include '${expected}'`);
    } else {
      ok(!tooltip.includes("Direction:"), "Tooltip should not include delay");
    }

    if (duration) {
      const expected = `Duration: ${duration}`;
      ok(tooltip.includes(expected), `Tooltip should include '${expected}'`);
    } else {
      ok(!tooltip.includes("Duration:"), "Tooltip should not include delay");
    }

    if (easing) {
      const expected = `Overall easing: ${easing}`;
      ok(tooltip.includes(expected), `Tooltip should include '${expected}'`);
    } else {
      ok(
        !tooltip.includes("Overall easing:"),
        "Tooltip should not include easing"
      );
    }

    if (endDelay) {
      const expected = `End delay: ${endDelay}`;
      ok(tooltip.includes(expected), `Tooltip should include '${expected}'`);
    } else {
      ok(
        !tooltip.includes("End delay:"),
        "Tooltip should not include endDelay"
      );
    }

    if (fill) {
      const expected = `Fill: ${fill}`;
      ok(tooltip.includes(expected), `Tooltip should include '${expected}'`);
    } else {
      ok(!tooltip.includes("Fill:"), "Tooltip should not include fill");
    }

    if (iterations) {
      const expected = `Repeats: ${iterations}`;
      ok(tooltip.includes(expected), `Tooltip should include '${expected}'`);
    } else {
      ok(
        !tooltip.includes("Repeats:"),
        "Tooltip should not include iterations"
      );
    }

    if (iterationStart) {
      const expected = `Iteration start: ${iterationStart}`;
      ok(tooltip.includes(expected), `Tooltip should include '${expected}'`);
    } else {
      ok(
        !tooltip.includes("Iteration start:"),
        "Tooltip should not include iterationStart"
      );
    }
  }
});
