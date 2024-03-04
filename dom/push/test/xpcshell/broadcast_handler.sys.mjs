export var broadcastHandler = {
  reset() {
    this.notifications = [];

    this.wasNotified = new Promise(resolve => {
      this.receivedBroadcastMessage = function () {
        resolve();
        this.notifications.push(Array.from(arguments));
      };
    });
  },
};
