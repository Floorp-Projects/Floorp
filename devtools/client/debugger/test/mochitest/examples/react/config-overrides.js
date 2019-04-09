// config-overrides.js
module.exports = {
  webpack: function(config, env) {
    if (env === "production") {
      //JS Overrides
      config.output.filename = '[name].js';
      config.output.chunkFilename = '[name].chunk.js';
    }

    // remove minifier
    const index = config.plugins.findIndex(o => o.options && o.options.compress)
    config.plugins.splice(index,1)

    return config;
  }
};
