export class ForceRefreshChild extends JSWindowActorChild {
  constructor() {
    super();
  }

  handleEvent(evt) {
    this.sendAsyncMessage("test:event", {
      type: evt.type,
      detail: evt.details,
    });
  }
}
