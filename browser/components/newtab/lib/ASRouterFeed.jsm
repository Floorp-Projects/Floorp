const {actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {ASRouter} = ChromeUtils.import("resource://activity-stream/lib/ASRouter.jsm", {});
ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * @class ASRouterFeed - Connects ASRouter singleton (see above) to Activity Stream's
 * store so that it can use the RemotePageManager.
 */
class ASRouterFeed {
  constructor(options = {}) {
    this.router = options.router || ASRouter;
  }

  async enable() {
    if (!this.router.initialized) {
      await this.router.init(
        this.store._messageChannel.channel,
        this.store.dbStorage.getDbTable("snippets"),
        this.store.dispatch
      );
    }
  }

  disable() {
    if (this.router.initialized) {
      this.router.uninit();
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.enable();
        break;
      case at.UNINIT:
        this.disable();
        break;
    }
  }
}
this.ASRouterFeed = ASRouterFeed;

const EXPORTED_SYMBOLS = ["ASRouterFeed"];
