'use strict';

module.exports = function(environment) {
  let ENV = {
    modulePrefix: 'quickstart',
    environment,
    locationType: 'auto',
    EmberENV: {
      FEATURES: {
        // Here you can enable experimental features on an ember canary build
        // e.g. 'with-controller': true
      },
      EXTEND_PROTOTYPES: {
        // Prevent Ember Data from overriding Date.parse.
        Date: false
      }
    },

    APP: {
      // Here you can pass flags/options to your application instance
      // when it is created
    },

    // NOTE(logan): Hard-code the URL for the debugger example to allow it to
    // function properly. The default "/" root makes assets fail to load.
    rootURL: "/browser/devtools/client/debugger/new/test/mochitest/examples/ember/quickstart/dist/",

    // If using "http://localhost:8000/integration/examples/ember/quickstart/dist/" to
    // access this test example, uncomment this line and re-run "yarn build".
    // rootURL: "/integration/examples/ember/quickstart/dist/",
  };

  if (environment === 'development') {
    // ENV.APP.LOG_RESOLVER = true;
    // ENV.APP.LOG_ACTIVE_GENERATION = true;
    // ENV.APP.LOG_TRANSITIONS = true;
    // ENV.APP.LOG_TRANSITIONS_INTERNAL = true;
    // ENV.APP.LOG_VIEW_LOOKUPS = true;
  }

  if (environment === 'test') {
    // Testem prefers this...
    ENV.locationType = 'none';

    // keep test console output quieter
    ENV.APP.LOG_ACTIVE_GENERATION = false;
    ENV.APP.LOG_VIEW_LOOKUPS = false;

    ENV.APP.rootElement = '#ember-testing';
    ENV.APP.autoboot = false;
  }

  if (environment === 'production') {
    // here you can enable a production-specific feature
  }

  return ENV;
};
