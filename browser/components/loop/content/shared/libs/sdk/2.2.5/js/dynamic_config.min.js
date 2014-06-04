!(function() {
  TB.Config.replaceWith({
    global: {
      exceptionLogging: {
        enabled: true,
        messageLimitPerPartner: 100
      },

      iceServers: {
        enabled: false
      },
    },

    partners: {
    }
  });
})(TB);
