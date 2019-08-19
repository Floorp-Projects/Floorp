/**
 * Common infrastructure for manifest tests.
 **/
"use strict";
const { ManifestProcessor } = SpecialPowers.Cu.import(
  "resource://gre/modules/ManifestProcessor.jsm"
);
const processor = ManifestProcessor;
const manifestURL = new URL(document.location.origin + "/manifest.json");
const docURL = document.location;
const seperators =
  "\u2028\u2029\u0020\u00A0\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000";
const lineTerminators = "\u000D\u000A\u2028\u2029";
const whiteSpace = `${seperators}${lineTerminators}`;
const typeTests = [1, null, {}, [], false];
const data = {
  jsonText: "{}",
  manifestURL,
  docURL,
};

const validThemeColors = [
  ["maroon", "#800000"],
  ["#f00", "#ff0000"],
  ["#ff0000", "#ff0000"],
  ["rgb(255,0,0)", "#ff0000"],
  ["rgb(255,0,0,1)", "#ff0000"],
  ["rgb(255,0,0,1.0)", "#ff0000"],
  ["rgb(255,0,0,100%)", "#ff0000"],
  ["rgb(255 0 0)", "#ff0000"],
  ["rgb(255 0 0 / 1)", "#ff0000"],
  ["rgb(255 0 0 / 1.0)", "#ff0000"],
  ["rgb(255 0 0 / 100%)", "#ff0000"],
  ["rgb(100%, 0%, 0%)", "#ff0000"],
  ["rgb(100%, 0%, 0%, 1)", "#ff0000"],
  ["rgb(100%, 0%, 0%, 1.0)", "#ff0000"],
  ["rgb(100%, 0%, 0%, 100%)", "#ff0000"],
  ["rgb(100% 0% 0%)", "#ff0000"],
  ["rgb(100% 0% 0% / 1)", "#ff0000"],
  ["rgb(100%, 0%, 0%, 1.0)", "#ff0000"],
  ["rgb(100%, 0%, 0%, 100%)", "#ff0000"],
  ["rgb(300,0,0)", "#ff0000"],
  ["rgb(300 0 0)", "#ff0000"],
  ["rgb(255,-10,0)", "#ff0000"],
  ["rgb(110%, 0%, 0%)", "#ff0000"],
  ["rgba(255,0,0)", "#ff0000"],
  ["rgba(255,0,0,1)", "#ff0000"],
  ["rgba(255 0 0 / 1)", "#ff0000"],
  ["rgba(100%,0%,0%,1)", "#ff0000"],
  ["rgba(0,0,255,0.5)", "#ff"],
  ["rgba(100%, 50%, 0%, 0.1)", "#ff8000"],
  ["hsl(120, 100%, 50%)", "#ff00"],
  ["hsl(120 100% 50%)", "#ff00"],
  ["hsl(120, 100%, 50%, 1.0)", "#ff00"],
  ["hsl(120 100% 50% / 1.0)", "#ff00"],
  ["hsla(120, 100%, 50%)", "#ff00"],
  ["hsla(120 100% 50%)", "#ff00"],
  ["hsla(120, 100%, 50%, 1.0)", "#ff00"],
  ["hsla(120 100% 50% / 1.0)", "#ff00"],
  ["hsl(120deg, 100%, 50%)", "#ff00"],
  ["hsl(133.33333333grad, 100%, 50%)", "#ff00"],
  ["hsl(2.0943951024rad, 100%, 50%)", "#ff00"],
  ["hsl(0.3333333333turn, 100%, 50%)", "#ff00"],
];

function setupManifest(key, value) {
  const manifest = {};
  manifest[key] = value;
  data.jsonText = JSON.stringify(manifest);
}

function testValidColors(key) {
  validThemeColors.forEach(item => {
    const [manifest_color, parsed_color] = item;
    setupManifest(key, manifest_color);
    const result = processor.process(data);

    is(
      result[key],
      parsed_color,
      `Expect ${key} to be returned for ${manifest_color}`
    );
  });

  // Trim tests
  validThemeColors.forEach(item => {
    const [manifest_color, parsed_color] = item;
    const expandedThemeColor = `${seperators}${lineTerminators}${manifest_color}${lineTerminators}${seperators}`;
    setupManifest(key, expandedThemeColor);
    const result = processor.process(data);

    is(
      result[key],
      parsed_color,
      `Expect trimmed ${key} to be returned for ${manifest_color}`
    );
  });
}

const invalidThemeColors = [
  "marooon",
  "f000000",
  "#ff00000",
  "rgb(100, 0%, 0%)",
  "rgb(255,0)",
  "rbg(255,-10,0)",
  "rgb(110, 0%, 0%)",
  "(255,0,0) }",
  "rgba(255)",
  " rgb(100%,0%,0%) }",
  "hsl(120, 100%, 50)",
  "hsl(120, 100%, 50.0)",
  "hsl 120, 100%, 50%",
  "hsla{120, 100%, 50%, 1}",
];

function testInvalidColors(key) {
  typeTests.forEach(type => {
    setupManifest(key, type);
    const result = processor.process(data);

    is(
      result[key],
      undefined,
      `Expect non-string ${key} to be undefined: ${typeof type}.`
    );
  });

  invalidThemeColors.forEach(manifest_color => {
    setupManifest(key, manifest_color);
    const result = processor.process(data);

    is(
      result[key],
      undefined,
      `Expect ${key} to be undefined: ${manifest_color}.`
    );
  });
}
