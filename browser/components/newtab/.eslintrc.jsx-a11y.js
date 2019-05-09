module.exports = {
  "plugins": [
    "jsx-a11y" // require("eslint-plugin-jsx-a11y")
  ],
  "extends": "plugin:jsx-a11y/recommended",
  "overrides": [{
    // These files use fluent-dom to insert content
    "files": [
      "content-src/asrouter/templates/OnboardingMessage/**",
      "content-src/asrouter/templates/Trailhead/**",
    ],
    "rules": {
      "jsx-a11y/anchor-has-content": 0,
      "jsx-a11y/heading-has-content": 0,
    }
  }],
};
