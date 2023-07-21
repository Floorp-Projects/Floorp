/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly when using layers

const HTML = `
  <style type='text/css'>
    @layer A, B;

    h1 {
      background-color: red;
      color: tomato !important;
    }

    @layer A {
      h1 {
        background-color: green;
        color: darkseagreen !important;
        color: lime !important;
        color: forestgreen;
      }
    }
    @layer B {
      h1 {
        background-color: cyan;
        color: blue !important;
      }
    }

    @layer {
      h2 {
        color: red !important;
      }
    }
    @layer {
      h2 {
        color: blue !important;
      }
    }

    @layer {
      @layer A {
        h3 {
          color: red !important;
        }
      }

      @layer A {
        h3 {
          color: lime !important;
        }
      }
    }

    @layer {
      @layer A {
        h3 {
          color: blue !important;
        }
      }
    }
  </style>
  <h1>Hello</h1>
  <h2>world</h2>
  <h3>!</h3>
`;

add_task(async function () {
  await addTab(
    `https://example.com/document-builder.sjs?html=${encodeURIComponent(HTML)}`
  );
  const { inspector, view } = await openRuleView();
  await selectNode("h1", inspector);

  info("Check background-color properties");
  is(
    await getComputedStyleProperty("h1", null, "background-color"),
    "rgb(255, 0, 0)",
    "The h1 element has a red background-color, as the value in the layer-less rule wins"
  );
  ok(
    !isPropertyOverridden(view, 1, { "background-color": "red" }),
    "background-color value in layer-less rule is not overridden"
  );
  ok(
    isPropertyOverridden(view, 2, { "background-color": "cyan" }),
    "background-color value in layer B rule is overridden"
  );
  ok(
    isPropertyOverridden(view, 3, { "background-color": "green" }),
    "background-color value in layer A rule is overridden"
  );

  info("Check (!important) color properties");
  is(
    await getComputedStyleProperty("h1", null, "color"),
    "rgb(0, 255, 0)",
    "The h1 element has a lime color, as the last important value in the first declared layer wins"
  );
  ok(
    isPropertyOverridden(view, 1, { color: "tomato" }),
    "important color value in layer-less rule is overridden"
  );
  ok(
    isPropertyOverridden(view, 2, { color: "blue" }),
    "important color value in layer B rule is overridden"
  );
  ok(
    isPropertyOverridden(view, 3, { color: "darkseagreen" }),
    "first important color value in layer A rule is overridden"
  );
  ok(
    !isPropertyOverridden(view, 3, { color: "lime" }),
    "important color value in layer A rule is not overridden"
  );
  ok(
    isPropertyOverridden(view, 3, { color: "forestgreen" }),
    "last, non-important color value in layer A rule is overridden"
  );

  info("Check (!important) color properties on nameless layers");
  await selectNode("h2", inspector);
  is(
    await getComputedStyleProperty("h2", null, "color"),
    "rgb(255, 0, 0)",
    "The h2 element has a blue color, as important value in the first nameless layer wins"
  );
  ok(
    isPropertyOverridden(view, 1, { color: "blue" }),
    "important color value in second layer-less rule is overridden"
  );
  ok(
    !isPropertyOverridden(view, 2, { color: "red" }),
    "important color value in first layer-less rule is not overridden"
  );

  info("Check (!important) color properties on nested layer in nameless layer");
  await selectNode("h3", inspector);
  is(
    await getComputedStyleProperty("h3", null, "color"),
    "rgb(0, 255, 0)",
    "The h3 element has a lime color, as important value in the last rule of the first declared nameless layer wins"
  );
  ok(
    isPropertyOverridden(view, 1, { color: "blue" }),
    "important color value in second layer-less rule is overridden"
  );
  ok(
    !isPropertyOverridden(view, 2, { color: "lime" }),
    "important color value in second rule of layer-less rule is not overridden"
  );
  ok(
    isPropertyOverridden(view, 3, { color: "red" }),
    "important color value in first rule of layer-less rule is overridden"
  );
});

function isPropertyOverridden(view, ruleIndex, property) {
  return getTextProperty(
    view,
    ruleIndex,
    property
  ).editor.element.classList.contains("ruleview-overridden");
}
