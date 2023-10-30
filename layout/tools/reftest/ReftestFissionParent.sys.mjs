export class ReftestFissionParent extends JSWindowActorParent {
  tellChildrenToFlushRendering(
    browsingContext,
    ignoreThrottledAnimations,
    needsAnimationFrame
  ) {
    let promises = [];
    this.tellChildrenToFlushRenderingRecursive(
      browsingContext,
      ignoreThrottledAnimations,
      needsAnimationFrame,
      promises
    );
    return Promise.allSettled(promises);
  }

  tellChildrenToFlushRenderingRecursive(
    browsingContext,
    ignoreThrottledAnimations,
    needsAnimationFrame,
    promises
  ) {
    let cwg = browsingContext.currentWindowGlobal;
    if (cwg && cwg.isProcessRoot) {
      let a = cwg.getActor("ReftestFission");
      if (a) {
        let responsePromise = a.sendQuery("FlushRendering", {
          ignoreThrottledAnimations,
          needsAnimationFrame,
        });
        promises.push(responsePromise);
      }
    }

    for (let context of browsingContext.children) {
      this.tellChildrenToFlushRenderingRecursive(
        context,
        ignoreThrottledAnimations,
        needsAnimationFrame,
        promises
      );
    }
  }

  // not including browsingContext
  getNearestProcessRootProperDescendants(browsingContext) {
    let result = [];
    for (let context of browsingContext.children) {
      this.getNearestProcessRootProperDescendantsRecursive(context, result);
    }
    return result;
  }

  getNearestProcessRootProperDescendantsRecursive(browsingContext, result) {
    let cwg = browsingContext.currentWindowGlobal;
    if (cwg && cwg.isProcessRoot) {
      result.push(browsingContext);
      return;
    }
    for (let context of browsingContext.children) {
      this.getNearestProcessRootProperDescendantsRecursive(context, result);
    }
  }

  // tell children and itself
  async tellChildrenToUpdateLayerTree(browsingContext) {
    let errorStrings = [];
    let infoStrings = [];

    let cwg = browsingContext.currentWindowGlobal;
    if (!cwg || !cwg.isProcessRoot) {
      if (cwg) {
        errorStrings.push(
          "tellChildrenToUpdateLayerTree called on a non process root?"
        );
      }
      return { errorStrings, infoStrings };
    }

    let actor = cwg.getActor("ReftestFission");
    if (!actor) {
      return { errorStrings, infoStrings };
    }

    // When we paint a document we also update the EffectsInfo visible rect in
    // nsSubDocumentFrame for any remote subdocuments. This visible rect is
    // used to limit painting for the subdocument in the subdocument's process.
    // So we want to ensure that the IPC message that updates the visible rect
    // to the subdocument's process arrives before we paint the subdocument
    // (otherwise our painting might not be up to date). We do this by sending,
    // and waiting for reply, an "EmptyMessage" to every direct descendant that
    // is in another process. Since we send the "EmptyMessage" after the
    // visible rect update message we know that the visible rect will be
    // updated by the time we hear back from the "EmptyMessage". Then we can
    // ask the subdocument process to paint.

    try {
      let result = await actor.sendQuery("UpdateLayerTree");
      errorStrings.push(...result.errorStrings);
    } catch (e) {
      infoStrings.push(
        "tellChildrenToUpdateLayerTree UpdateLayerTree msg to child rejected: " +
          e
      );
    }

    let descendants =
      actor.getNearestProcessRootProperDescendants(browsingContext);
    for (let context of descendants) {
      let cwg2 = context.currentWindowGlobal;
      if (cwg2) {
        if (!cwg2.isProcessRoot) {
          errorStrings.push(
            "getNearestProcessRootProperDescendants returned a non process root?"
          );
        }
        let actor2 = cwg2.getActor("ReftestFission");
        if (actor2) {
          try {
            await actor2.sendQuery("EmptyMessage");
          } catch (e) {
            infoStrings.push(
              "tellChildrenToUpdateLayerTree EmptyMessage msg to child rejected: " +
                e
            );
          }

          try {
            let result2 = await actor2.tellChildrenToUpdateLayerTree(context);
            errorStrings.push(...result2.errorStrings);
            infoStrings.push(...result2.infoStrings);
          } catch (e) {
            errorStrings.push(
              "tellChildrenToUpdateLayerTree recursive tellChildrenToUpdateLayerTree call rejected: " +
                e
            );
          }
        }
      }
    }

    return { errorStrings, infoStrings };
  }

  tellChildrenToSetupDisplayport(browsingContext, promises) {
    let cwg = browsingContext.currentWindowGlobal;
    if (cwg && cwg.isProcessRoot) {
      let a = cwg.getActor("ReftestFission");
      if (a) {
        let responsePromise = a.sendQuery("SetupDisplayport");
        promises.push(responsePromise);
      }
    }

    for (let context of browsingContext.children) {
      this.tellChildrenToSetupDisplayport(context, promises);
    }
  }

  tellChildrenToSetupAsyncScrollOffsets(
    browsingContext,
    allowFailure,
    promises
  ) {
    let cwg = browsingContext.currentWindowGlobal;
    if (cwg && cwg.isProcessRoot) {
      let a = cwg.getActor("ReftestFission");
      if (a) {
        let responsePromise = a.sendQuery("SetupAsyncScrollOffsets", {
          allowFailure,
        });
        promises.push(responsePromise);
      }
    }

    for (let context of browsingContext.children) {
      this.tellChildrenToSetupAsyncScrollOffsets(
        context,
        allowFailure,
        promises
      );
    }
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "ForwardAfterPaintEvent": {
        let cwg = msg.data.toBrowsingContext.currentWindowGlobal;
        if (cwg) {
          let a = cwg.getActor("ReftestFission");
          if (a) {
            a.sendAsyncMessage(
              "ForwardAfterPaintEventToSelfAndParent",
              msg.data
            );
          }
        }
        break;
      }
      case "FlushRendering": {
        let promise = this.tellChildrenToFlushRendering(
          msg.data.browsingContext,
          msg.data.ignoreThrottledAnimations,
          msg.data.needsAnimationFrame
        );
        return promise.then(function (results) {
          let errorStrings = [];
          let warningStrings = [];
          let infoStrings = [];
          for (let r of results) {
            if (r.status != "fulfilled") {
              if (r.status == "pending") {
                errorStrings.push(
                  "FlushRendering sendQuery to child promise still pending?"
                );
              } else {
                // We expect actors to go away causing sendQuery's to fail, so
                // just note it.
                infoStrings.push(
                  "FlushRendering sendQuery to child promise rejected: " +
                    r.reason
                );
              }
              continue;
            }

            errorStrings.push(...r.value.errorStrings);
            warningStrings.push(...r.value.warningStrings);
            infoStrings.push(...r.value.infoStrings);
          }
          return { errorStrings, warningStrings, infoStrings };
        });
      }
      case "UpdateLayerTree": {
        return this.tellChildrenToUpdateLayerTree(msg.data.browsingContext);
      }
      case "TellChildrenToSetupDisplayport": {
        let promises = [];
        this.tellChildrenToSetupDisplayport(msg.data.browsingContext, promises);
        return Promise.allSettled(promises).then(function (results) {
          let errorStrings = [];
          let infoStrings = [];
          for (let r of results) {
            if (r.status != "fulfilled") {
              // We expect actors to go away causing sendQuery's to fail, so
              // just note it.
              infoStrings.push(
                "SetupDisplayport sendQuery to child promise rejected: " +
                  r.reason
              );
              continue;
            }

            errorStrings.push(...r.value.errorStrings);
            infoStrings.push(...r.value.infoStrings);
          }
          return { errorStrings, infoStrings };
        });
      }

      case "SetupAsyncScrollOffsets": {
        let promises = [];
        this.tellChildrenToSetupAsyncScrollOffsets(
          this.manager.browsingContext,
          msg.data.allowFailure,
          promises
        );
        return Promise.allSettled(promises).then(function (results) {
          let errorStrings = [];
          let infoStrings = [];
          let updatedAny = false;
          for (let r of results) {
            if (r.status != "fulfilled") {
              // We expect actors to go away causing sendQuery's to fail, so
              // just note it.
              infoStrings.push(
                "SetupAsyncScrollOffsets sendQuery to child promise rejected: " +
                  r.reason
              );
              continue;
            }

            errorStrings.push(...r.value.errorStrings);
            infoStrings.push(...r.value.infoStrings);
            if (r.value.updatedAny) {
              updatedAny = true;
            }
          }
          return { errorStrings, infoStrings, updatedAny };
        });
      }
    }
    return undefined;
  }
}
