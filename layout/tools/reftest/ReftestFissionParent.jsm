var EXPORTED_SYMBOLS = ["ReftestFissionParent"];

class ReftestFissionParent extends JSWindowActorParent {

  tellChildrenToFlushRendering(browsingContext, ignoreThrottledAnimations) {
    let promises = [];
    this.tellChildrenToFlushRenderingRecursive(browsingContext, ignoreThrottledAnimations, promises);
    return Promise.allSettled(promises);
  }

  tellChildrenToFlushRenderingRecursive(browsingContext, ignoreThrottledAnimations, promises) {
    let cwg = browsingContext.currentWindowGlobal;
    if (cwg && cwg.isProcessRoot) {
      let a = cwg.getActor("ReftestFission");
      if (a) {
        let responsePromise = a.sendQuery("FlushRendering", {ignoreThrottledAnimations});
        promises.push(responsePromise);
      }
    }

    for (let context of browsingContext.getChildren()) {
      this.tellChildrenToFlushRenderingRecursive(context, ignoreThrottledAnimations, promises);
    }
  }

  tellChildrenToUpdateLayerTree(browsingContext) {
    let promises = [];
    this.tellChildrenToUpdateLayerTreeRecursive(browsingContext, promises);
    return Promise.allSettled(promises);
  }

  tellChildrenToUpdateLayerTreeRecursive(browsingContext, promises) {
    let cwg = browsingContext.currentWindowGlobal;
    if (cwg && cwg.isProcessRoot) {
      let a = cwg.getActor("ReftestFission");
      if (a) {
        let responsePromise = a.sendQuery("UpdateLayerTree");
        promises.push(responsePromise);
      }
    }

    for (let context of browsingContext.getChildren()) {
      this.tellChildrenToUpdateLayerTreeRecursive(context, promises);
    }
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "FlushRendering":
      {
        let promise = this.tellChildrenToFlushRendering(msg.data.browsingContext, msg.data.ignoreThrottledAnimations);
        return promise.then(function (results) {
          let errorStrings = [];
          let warningStrings = [];
          let infoStrings = [];
          for (let r of results) {
            if (r.status != "fulfilled") {
              if (r.status == "pending") {
                errorStrings.push("FlushRendering sendQuery to child promise still pending?");
              } else {
                // We expect actors to go away causing sendQuery's to fail, so
                // just note it.
                infoStrings.push("FlushRendering sendQuery to child promise rejected: " + r.reason);
              }
              continue;
            }

            errorStrings.concat(r.value.errorStrings);
            warningStrings.concat(r.value.warningStrings);
            infoStrings.concat(r.value.infoStrings);
          }
          return {errorStrings, warningStrings, infoStrings};
        });
      }
      case "UpdateLayerTree":
      {
        let promise = this.tellChildrenToUpdateLayerTree(msg.data.browsingContext);
        return promise.then(function (results) {
          let errorStrings = [];
          for (let r of results) {
            if (r.status != "fulfilled") {
              if (r.status == "pending") {
                errorStrings.push("UpdateLayerTree sendQuery to child promise still pending?");
              } else {
                // We expect actors to go away causing sendQuery's to fail, so
                // just note it.
                infoStrings.push("UpdateLayerTree sendQuery to child promise rejected: " + r.reason);
              }
              continue;
            }

            if (r.value.errorString != null) {
              errorStrings.push(r.value.errorString);
            }
          }
          return errorStrings;
        });
      }
    }
  }

}
