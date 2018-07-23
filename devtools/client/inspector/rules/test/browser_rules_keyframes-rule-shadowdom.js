/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that keyframes are displayed for elements nested under a shadow-root.

const TEST_URI = `data:text/html;charset=utf-8,
  <div></div>
  <script>
    document.querySelector('div').attachShadow({mode: 'open'}).innerHTML = \`
      <span>text</span>
      <style>
        @keyframes blink {
          0% {
            border: rgba(255,0,0,1) 2px dashed;
          }
          100% {
            border: rgba(255,0,0,0) 2px dashed;
          }
        }
        span {
          animation: blink .5s 0s infinite;
        }
      </style>\`;
  </script>`;

add_task(async function() {
  await enableWebComponents();

  await addTab(TEST_URI);

  const {inspector, view} = await openRuleView();

  info("Expand the shadow-root parent");
  const divFront = await getNodeFront("div", inspector);
  await inspector.markup.expandNode(divFront);
  await waitForMultipleChildrenUpdates(inspector);

  const {markup} = inspector;
  const divContainer = markup.getContainer(divFront);

  info("Expand the shadow-root");
  const shadowRootContainer = divContainer.getChildContainers()[0];
  await expandContainer(inspector, shadowRootContainer);

  info("Retrieve the rules displayed for the span under the shadow root");
  const spanContainer = shadowRootContainer.getChildContainers()[0];
  const rules = await getKeyframeRules(spanContainer.node, inspector, view);

  is(convertTextPropsToString(rules.keyframeRules[0].textProps),
    "border: rgba(255,0,0,1) 2px dashed",
    "Keyframe blink (0%) property is correct"
  );

  is(convertTextPropsToString(rules.keyframeRules[1].textProps),
    "border: rgba(255,0,0,0) 2px dashed",
    "Keyframe blink (100%) property is correct"
  );
});

function convertTextPropsToString(textProps) {
  return textProps.map(t => t.name + ": " + t.value).join("; ");
}

async function getKeyframeRules(selector, inspector, view) {
  await selectNode(selector, inspector);
  const elementStyle = view._elementStyle;

  const rules = {
    elementRules: elementStyle.rules.filter(rule => !rule.keyframes),
    keyframeRules: elementStyle.rules.filter(rule => rule.keyframes)
  };

  return rules;
}
