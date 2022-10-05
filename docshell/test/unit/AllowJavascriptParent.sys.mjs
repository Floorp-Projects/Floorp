let loadPromises = new WeakMap();

export class AllowJavascriptParent extends JSWindowActorParent {
  async receiveMessage(msg) {
    switch (msg.name) {
      case "LoadFired":
        let bc = this.browsingContext;
        let deferred = loadPromises.get(bc);
        if (deferred) {
          loadPromises.delete(bc);
          deferred.resolve(this);
        }
        break;
    }
  }

  static promiseLoad(bc) {
    let deferred = loadPromises.get(bc);
    if (!deferred) {
      deferred = {};
      deferred.promise = new Promise(resolve => {
        deferred.resolve = resolve;
      });
      loadPromises.set(bc, deferred);
    }
    return deferred.promise;
  }
}
