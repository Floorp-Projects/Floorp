import { CFRMessageProvider } from "modules/CFRMessageProvider.sys.mjs";

describe("CFRMessageProvider", () => {
  let messages;
  beforeEach(async () => {
    messages = await CFRMessageProvider.getMessages();
  });
  it("should have messages", () => assert.ok(messages.length));
});
