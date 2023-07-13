/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for the order of matched selector in the computed view.
const TEST_URI = URL_ROOT + "doc_matched_selectors.html";

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openComputedView();

  const checkMatchedSelectors = options =>
    checkBackgroundColorMatchedSelectors(inspector, view, options);

  info("matching rules with different specificity");
  await checkMatchedSelectors({
    elementAttributes: {
      id: "specificity",
      class: "mySection",
    },
    style: `
      #specificity.mySection {
        --spec_highest: var(--winning-color);
        background-color: var(--spec_highest);
      }
      #specificity {
        background-color: var(--spec_lowest);
      }`,
    expectedMatchedSelectors: [
      // Higher specificity wins
      { selector: "#specificity.mySection", value: "var(--spec_highest)" },
      { selector: "#specificity", value: "var(--spec_lowest)" },
    ],
  });

  info("matching rules with same specificity");
  await checkMatchedSelectors({
    elementAttributes: {
      id: "order-of-appearance",
    },
    style: `
      #order-of-appearance {
        background-color: var(--appearance-order_first);
      }
      #order-of-appearance {
        --appearance-order_second: var(--winning-color);
        background-color: var(--appearance-order_second);
      }`,
    expectedMatchedSelectors: [
      // Last rule in stylesheet wins
      {
        selector: "#order-of-appearance",
        value: "var(--appearance-order_second)",
      },
      {
        selector: "#order-of-appearance",
        value: "var(--appearance-order_first)",
      },
    ],
  });

  info("matching rules on element with style attribute");
  await checkMatchedSelectors({
    elementAttributes: {
      id: "style-attr",
      style: "background-color: var(--style-attr_in-attr)",
    },
    style: `
      main {
        --style-attr_in-attr: var(--winning-color);
      }

      #style-attr {
        background-color: var(--style-attr_in-rule);
      }
    `,
    expectedMatchedSelectors: [
      // style attribute wins
      { selector: "this.style", value: "var(--style-attr_in-attr)" },
      { selector: "#style-attr", value: "var(--style-attr_in-rule)" },
    ],
  });

  info("matching rules on different layers");
  await checkMatchedSelectors({
    elementAttributes: {
      id: "layers",
      class: "layers",
    },
    style: `
      @layer second {
        .layers {
          --layers_in-second: var(--winning-color);
          background-color: var(--layers_in-second);
        }
      }
      @layer first {
        #layers {
          background-color: var(--layers_in-first);
        }
      }
    `,
    expectedMatchedSelectors: [
      // rule in last declared layer wins
      { selector: ".layers", value: "var(--layers_in-second)" },
      { selector: "#layers", value: "var(--layers_in-first)" },
    ],
  });

  info("matching rules on same layer, with same specificity");
  await checkMatchedSelectors({
    elementAttributes: {
      id: "same-layers-order-of-appearance",
    },
    style: `
      @layer second {
        #same-layers-order-of-appearance {
          background-color: var(--same-layers-appearance-order_first);
        }

        #same-layers-order-of-appearance {
          --same-layers-appearance-order_second: var(--winning-color);
          background-color: var(--same-layers-appearance-order_second);
        }
      }
    `,
    expectedMatchedSelectors: [
      // last rule in the layer wins
      {
        selector: "#same-layers-order-of-appearance",
        value: "var(--same-layers-appearance-order_second)",
      },
      {
        selector: "#same-layers-order-of-appearance",
        value: "var(--same-layers-appearance-order_first)",
      },
    ],
  });

  info("matching rules some in layers, some not");
  await checkMatchedSelectors({
    elementAttributes: {
      id: "in-layer-and-no-layer",
    },
    style: `
      @layer second {
        #in-layer-and-no-layer {
          background-color: var(--in-layer-and-no-layer_in-second);
        }
      }

      @layer first {
        #in-layer-and-no-layer {
          background-color: var(--in-layer-and-no-layer_in-first);
        }
      }

      #in-layer-and-no-layer {
        --in-layer-and-no-layer_no-layer: var(--winning-color);
        background-color: var(--in-layer-and-no-layer_no-layer);
      }`,
    expectedMatchedSelectors: [
      // rule not in layer wins
      {
        selector: "#in-layer-and-no-layer",
        value: "var(--in-layer-and-no-layer_no-layer)",
      },
      {
        selector: "#in-layer-and-no-layer",
        value: "var(--in-layer-and-no-layer_in-second)",
      },
      {
        selector: "#in-layer-and-no-layer",
        value: "var(--in-layer-and-no-layer_in-first)",
      },
    ],
  });

  info(
    "matching rules with different specificity and one property declared with !important"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "important-specificity",
      class: "myImportantSection",
    },
    style: `
      #important-specificity.myImportantSection {
        background-color: var(--important-spec_highest);
      }
      #important-specificity {
        --important-spec_lowest-important: var(--winning-color);
        background-color: var(--important-spec_lowest-important) !important;
      }`,
    expectedMatchedSelectors: [
      // lesser specificity, but value was set with !important
      {
        selector: "#important-specificity",
        value: "var(--important-spec_lowest-important)",
      },
      {
        selector: "#important-specificity.myImportantSection",
        value: "var(--important-spec_highest)",
      },
    ],
  });

  info(
    "matching rules with different specificity and all properties declared with !important"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-specificity",
      class: "myAllImportantSection",
    },
    style: `
      #all-important-specificity.myAllImportantSection {
        --all-important-spec_highest-important: var(--winning-color);
        background-color: var(--all-important-spec_highest-important) !important;
      }
      #all-important-specificity {
        background-color: var(--all-important-spec_lowest-important) !important;
      }`,
    expectedMatchedSelectors: [
      // all values !important, so highest specificity rule wins
      {
        selector: "#all-important-specificity.myAllImportantSection",
        value: "var(--all-important-spec_highest-important)",
      },
      {
        selector: "#all-important-specificity",
        value: "var(--all-important-spec_lowest-important)",
      },
    ],
  });

  info(
    "matching rules with same specificity and one property declared with !important"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "important-order-of-appearance",
    },
    style: `
      #important-order-of-appearance {
        --important-appearance-order_first-important: var(--winning-color);
        background-color: var(--important-appearance-order_first-important) !important;
      }
      #important-order-of-appearance {
        background-color: var(--important-appearance-order_second);
      }`,
    expectedMatchedSelectors: [
      // same specificity, but this value was set with !important
      {
        selector: "#important-order-of-appearance",
        value: "var(--important-appearance-order_first-important)",
      },
      {
        selector: "#important-order-of-appearance",
        value: "var(--important-appearance-order_second)",
      },
    ],
  });

  info(
    "matching rules with same specificity and all properties declared with !important"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-order-of-appearance",
    },
    style: `
      #all-important-order-of-appearance {
        background-color: var(--all-important-appearance-order_first-important) !important;
      }
      #all-important-order-of-appearance {
        --all-important-appearance-order_second-important: var(--winning-color);
        background-color: var(--all-important-appearance-order_second-important) !important;
      }`,
    expectedMatchedSelectors: [
      // all values !important, so latest rule in stylesheet wins
      {
        selector: "#all-important-order-of-appearance",
        value: "var(--all-important-appearance-order_second-important)",
      },
      {
        selector: "#all-important-order-of-appearance",
        value: "var(--all-important-appearance-order_first-important)",
      },
    ],
  });

  info(
    "matching rules with important property on element with style attribute"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "important-style-attr",
      style: "background-color: var(--important-style-attr_in-attr);",
    },
    style: `
      #important-style-attr {
        --important-style-attr_in-rule-important: var(--winning-color);
        background-color: var(--important-style-attr_in-rule-important) !important;
      }`,
    expectedMatchedSelectors: [
      // important property wins over style attribute
      {
        selector: "#important-style-attr",
        value: "var(--important-style-attr_in-rule-important)",
      },
      { selector: "this.style", value: "var(--important-style-attr_in-attr)" },
    ],
  });

  info(
    "matching rules with important property on element with style attribute and important value"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-style-attr",
      style:
        "background-color: var(--all-important-style-attr_in-attr-important) !important;",
    },
    style: `
      main {
        --all-important-style-attr_in-attr-important: var(--winning-color);
      }
      #all-important-style-attr {
        background-color: var(--all-important-style-attr_in-rule-important);
      }`,
    expectedMatchedSelectors: [
      // both values are important, so style attribute wins
      {
        selector: "this.style",
        value: "var(--all-important-style-attr_in-attr-important)",
      },
      {
        selector: "#all-important-style-attr",
        value: "var(--all-important-style-attr_in-rule-important)",
      },
    ],
  });

  info(
    "matching rules on different layer, with same specificity and important values"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "important-layers",
    },
    style: `
      @layer second {
        #important-layers {
          background-color: var(--important-layers_in-second);
        }
      }
      @layer first {
        #important-layers {
          --important-layers_in-first-important: var(--winning-color);
          background-color: var(--important-layers_in-first-important) !important;
        }
      }`,
    expectedMatchedSelectors: [
      // rule with important property wins
      {
        selector: "#important-layers",
        value: "var(--important-layers_in-first-important)",
      },
      {
        selector: "#important-layers",
        value: "var(--important-layers_in-second)",
      },
    ],
  });

  info(
    "matching rules on different layer, with same specificity and all important values"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-layers",
    },
    style: `
      @layer second {
        #all-important-layers {
          background-color: var(--all-important-layers_in-second-important) !important;
        }
      }
      @layer first {
        #all-important-layers {
          --all-important-layers_in-first-important: var(--winning-color);
          background-color: var(--all-important-layers_in-first-important) !important;
        }
      }`,
    expectedMatchedSelectors: [
      // all properties are important, rule from first declared layer wins
      {
        selector: "#all-important-layers",
        value: "var(--all-important-layers_in-first-important)",
      },
      {
        selector: "#all-important-layers",
        value: "var(--all-important-layers_in-second-important)",
      },
    ],
  });

  info(
    "matching rules on same layer, with same specificity and important values"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "important-same-layers-order-of-appearance",
    },
    style: `
      @layer second {
        #important-same-layers-order-of-appearance {
          --important-same-layers-appearance-order_first-important: var(--winning-color);
          background-color: var(--important-same-layers-appearance-order_first-important) !important;
        }

        #important-same-layers-order-of-appearance {
          background-color: var(--important-same-layers-appearance-order_second);
        }
      }`,
    expectedMatchedSelectors: [
      // rule with important property wins
      {
        selector: "#important-same-layers-order-of-appearance",
        value: "var(--important-same-layers-appearance-order_first-important)",
      },
      {
        selector: "#important-same-layers-order-of-appearance",
        value: "var(--important-same-layers-appearance-order_second)",
      },
    ],
  });

  info(
    "matching rules on same layer, with same specificity and all important values"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-same-layers-order-of-appearance",
    },
    style: `
      @layer second {
        #all-important-same-layers-order-of-appearance {
          background-color: var(--all-important-same-layers-appearance-order_first-important) !important;
        }

        #all-important-same-layers-order-of-appearance {
          --all-important-same-layers-appearance-order_second-important: var(--winning-color);
          background-color: var(--all-important-same-layers-appearance-order_second-important) !important;
        }
      }`,
    expectedMatchedSelectors: [
      // last rule with important property wins
      {
        selector: "#all-important-same-layers-order-of-appearance",
        value:
          "var(--all-important-same-layers-appearance-order_second-important)",
      },
      {
        selector: "#all-important-same-layers-order-of-appearance",
        value:
          "var(--all-important-same-layers-appearance-order_first-important)",
      },
    ],
  });

  info("matching rules ,some in layers, some not, important values in layers");
  await checkMatchedSelectors({
    elementAttributes: {
      id: "important-in-layer-and-no-layer",
    },
    style: `
      @layer second {
        #important-in-layer-and-no-layer {
          background-color: var(--important-in-layer-and-no-layer_in-second);
        }
      }

      @layer first {
        #important-in-layer-and-no-layer {
          --important-in-layer-and-no-layer_in-first-important: var(--winning-color);
          background-color: var(--important-in-layer-and-no-layer_in-first-important) !important;
        }
      }

      #important-in-layer-and-no-layer {
        background-color: var(--important-in-layer-and-no-layer_no-layer);
      }`,
    expectedMatchedSelectors: [
      // rule with important property wins
      {
        selector: "#important-in-layer-and-no-layer",
        value: "var(--important-in-layer-and-no-layer_in-first-important)",
      },
      // then rule not in layer
      {
        selector: "#important-in-layer-and-no-layer",
        value: "var(--important-in-layer-and-no-layer_no-layer)",
      },
      {
        selector: "#important-in-layer-and-no-layer",
        value: "var(--important-in-layer-and-no-layer_in-second)",
      },
    ],
  });

  info("matching rules ,some in layers, some not, all important values");
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-in-layer-and-no-layer",
    },
    style: `
      @layer second {
        #all-important-in-layer-and-no-layer {
          background-color: var(--all-important-in-layer-and-no-layer_in-second-important) !important;
        }
      }

      @layer first {
        #all-important-in-layer-and-no-layer {
          --all-important-in-layer-and-no-layer_in-first-important: var(--winning-color);
          background-color: var(--all-important-in-layer-and-no-layer_in-first-important) !important;
        }
      }

      #all-important-in-layer-and-no-layer {
        background-color: var(--all-important-in-layer-and-no-layer_no-layer-important) !important;
      }`,
    expectedMatchedSelectors: [
      // important properties in first declared layer wins
      {
        selector: "#all-important-in-layer-and-no-layer",
        value: "var(--all-important-in-layer-and-no-layer_in-first-important)",
      },
      // then following important rules in layers
      {
        selector: "#all-important-in-layer-and-no-layer",
        value: "var(--all-important-in-layer-and-no-layer_in-second-important)",
      },
      // then important rules not in layers
      {
        selector: "#all-important-in-layer-and-no-layer",
        value: "var(--all-important-in-layer-and-no-layer_no-layer-important)",
      },
    ],
  });

  info(
    "matching rules ,some in layers, some not, and style attribute all important values"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-in-layer-no-layer-style-attr",
      style:
        "background-color: var(--all-important-in-layer-no-layer-style-attr_in-attr-important) !important",
    },
    style: `
      main {
        --all-important-in-layer-no-layer-style-attr_in-attr-important: var(--winning-color);
      }

      @layer second {
        #all-important-in-layer-no-layer-style-attr {
          background-color: var(--all-important-in-layer-no-layer-style-attr_in-second-important) !important;
        }
      }

      @layer first {
        #all-important-in-layer-no-layer-style-attr {
          background-color: var(--all-important-in-layer-no-layer-style-attr_in-first-important) !important;
        }
      }

      #all-important-in-layer-no-layer-style-attr {
        background-color: var(--all-important-in-layer-no-layer-style-attr_no-layer-important) !important;
      }`,
    expectedMatchedSelectors: [
      // important properties in style attribute wins
      {
        selector: "this.style",
        value:
          "var(--all-important-in-layer-no-layer-style-attr_in-attr-important)",
      },
      // then important property in first declared layer
      {
        selector: "#all-important-in-layer-no-layer-style-attr",
        value:
          "var(--all-important-in-layer-no-layer-style-attr_in-first-important)",
      },
      // then following important property in layers
      {
        selector: "#all-important-in-layer-no-layer-style-attr",
        value:
          "var(--all-important-in-layer-no-layer-style-attr_in-second-important)",
      },
      // then important property not in layers
      {
        selector: "#all-important-in-layer-no-layer-style-attr",
        value:
          "var(--all-important-in-layer-no-layer-style-attr_no-layer-important)",
      },
    ],
  });

  info(
    "matching rules on same layer but different rules and all important values"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-same-layer-different-rule",
    },
    style: `
      @layer first {
        #all-important-same-layer-different-rule {
          background-color: var(--all-important-same-layer-different-rule_first-important) !important;
        }
      }

      @layer first {
        #all-important-same-layer-different-rule {
          --all-important-same-layer-different-rule_second-important: var(--winning-color);
          background-color: var(--all-important-same-layer-different-rule_second-important) !important;
        }
      }`,
    expectedMatchedSelectors: [
      // last rule for the layer with important property wins
      {
        selector: "#all-important-same-layer-different-rule",
        value:
          "var(--all-important-same-layer-different-rule_second-important)",
      },
      {
        selector: "#all-important-same-layer-different-rule",
        value: "var(--all-important-same-layer-different-rule_first-important)",
      },
    ],
  });

  info(
    "matching rules on same layer but different nested rules and all important values"
  );
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-same-nested-layer-different-rule",
    },
    style: `
      @layer first {
        @layer {
          @layer second {
            #all-important-same-nested-layer-different-rule {
              background-color: var(--all-important-same-nested-layer-different-rule_first-important) !important;
            }
          }

          @layer second {
            #all-important-same-nested-layer-different-rule {
              --all-important-same-nested-layer-different-rule_second-important: var(--winning-color);
              background-color: var(--all-important-same-nested-layer-different-rule_second-important) !important;
            }
          }
        }
      }`,
    expectedMatchedSelectors: [
      // last rule for the layer with important property wins
      {
        selector: "#all-important-same-nested-layer-different-rule",
        value:
          "var(--all-important-same-nested-layer-different-rule_second-important)",
      },
      {
        selector: "#all-important-same-nested-layer-different-rule",
        value:
          "var(--all-important-same-nested-layer-different-rule_first-important)",
      },
    ],
  });

  info("matching rules on different nameless layers and all important values");
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-different-nameless-layers",
    },
    style: `
      @layer {
        @layer first {
          #all-important-different-nameless-layers {
            --all-important-different-nameless-layers_first-important: var(--winning-color);
            background-color: var(--all-important-different-nameless-layers_first-important) !important;
          }
        }
      }
      @layer {
        @layer first {
          #all-important-different-nameless-layers {
            background-color: var(--all-important-different-nameless-layers_second-important) !important;
          }
        }
      }`,
    expectedMatchedSelectors: [
      // rule with important property in first declared layer wins
      {
        selector: "#all-important-different-nameless-layers",
        value: "var(--all-important-different-nameless-layers_first-important)",
      },
      {
        selector: "#all-important-different-nameless-layers",
        value:
          "var(--all-important-different-nameless-layers_second-important)",
      },
    ],
  });

  info("matching rules on different imported layers");
  // no provided style as rules are defined in doc_matched_selectors_imported_*.css
  await checkMatchedSelectors({
    elementAttributes: {
      id: "imported-layers",
    },
    expectedMatchedSelectors: [
      // rule in last declared layer wins
      {
        selector: "#imported-layers",
        value: "var(--imported-layers_in-anonymous-second)",
      },
      {
        selector: "#imported-layers",
        value: "var(--imported-layers_in-nested-importedSecond)",
      },
      {
        selector: "#imported-layers",
        value: "var(--imported-layers_in-anonymous-first)",
      },
      {
        selector: "#imported-layers",
        value: "var(--imported-layers_in-importedSecond)",
      },
      {
        selector: "#imported-layers",
        value: "var(--imported-layers_in-importedFirst-second)",
      },
      {
        selector: "#imported-layers",
        value: "var(--imported-layers_in-importedFirst-first)",
      },
    ],
  });

  info("matching rules on different imported layers all with important values");
  // no provided style as rules are defined in doc_matched_selectors_imported_*.css
  await checkMatchedSelectors({
    elementAttributes: {
      id: "all-important-imported-layers",
    },
    expectedMatchedSelectors: [
      // last important property in first declared layer wins
      {
        selector: "#all-important-imported-layers",
        value:
          "var(--all-important-imported-layers_in-importedFirst-second-important)",
      },
      // then earlier important property for first declared layer
      {
        selector: "#all-important-imported-layers",
        value:
          "var(--all-important-imported-layers_in-importedFirst-first-important)",
      },
      {
        selector: "#all-important-imported-layers",
        value:
          "var(--all-important-imported-layers_in-importedSecond-important)",
      },
      {
        selector: "#all-important-imported-layers",
        value:
          "var(--all-important-imported-layers_in-anonymous-first-important)",
      },
      {
        selector: "#all-important-imported-layers",
        value:
          "var(--all-important-imported-layers_in-nested-importedSecond-important)",
      },
      {
        selector: "#all-important-imported-layers",
        value:
          "var(--all-important-imported-layers_in-anonymous-second-important)",
      },
    ],
  });
});

async function checkBackgroundColorMatchedSelectors(
  inspector,
  view,
  { elementAttributes, style, expectedMatchedSelectors }
) {
  const elementId = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [elementAttributes, style],
    (attr, _style) => {
      const sectionEl = content.document.createElement("section");
      for (const [name, value] of Object.entries(attr)) {
        sectionEl.setAttribute(name, value);
      }

      if (_style) {
        const styleEl = content.document.createElement("style");
        styleEl.innerText = _style;
        styleEl.setAttribute("id", `style-${sectionEl.id}`);
        content.document.head.append(styleEl);
      }
      content.document.querySelector("main").append(sectionEl);

      return sectionEl.id;
    }
  );
  const selector = `#${elementId}`;
  await selectNode(selector, inspector);

  const bgColorComputedValue = await getComputedStyleProperty(
    selector,
    null,
    "background-color"
  );
  is(
    bgColorComputedValue,
    "rgb(0, 0, 255)",
    `The created element does have a "blue" background-color`
  );

  const propertyView = getPropertyView(view, "background-color");
  ok(propertyView, "found PropertyView for background-color");
  const valueNode = propertyView.valueNode.querySelector(".computed-color");
  is(
    valueNode.textContent,
    "rgb(0, 0, 255)",
    `The displayed computed value is the expected "blue"`
  );

  is(propertyView.hasMatchedSelectors, true, "hasMatchedSelectors is true");

  info("Expanding the matched selectors");
  propertyView.matchedExpanded = true;
  await propertyView.refreshMatchedSelectors();

  const selectorsEl =
    propertyView.matchedSelectorsContainer.querySelectorAll(".rule-text");
  is(
    selectorsEl.length,
    expectedMatchedSelectors.length,
    "Expected number of selectors are displayed"
  );

  selectorsEl.forEach((selectorEl, index) => {
    is(
      selectorEl.querySelector(".fix-get-selection").innerText,
      expectedMatchedSelectors[index].selector,
      `Selector #${index} is the expected one`
    );
    is(
      selectorEl.querySelector(".computed-other-property-value").innerText,
      expectedMatchedSelectors[index].value,
      `Selector #${index} has the expected background color`
    );
    const classToMatch = index === 0 ? "bestmatch" : "matched";
    ok(
      selectorEl.classList.contains(classToMatch),
      `selector element has expected "${classToMatch}" class`
    );
  });

  // cleanup
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [elementId], id => {
    // Remove added element and stylesheet
    content.document.getElementById(id).remove();
    // Some test cases don't insert a style element
    content.document.getElementById(`style-${id}`)?.remove();
  });
}

function getPropertyView(computedView, name) {
  return computedView.propertyViews.find(view => view.name === name);
}
