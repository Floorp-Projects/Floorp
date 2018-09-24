export const CHILD_TO_PARENT_MESSAGE_NAME = "ASRouter:child-to-parent";
export const PARENT_TO_CHILD_MESSAGE_NAME = "ASRouter:parent-to-child";

export const FAKE_LOCAL_MESSAGES = [
  {id: "foo", template: "simple_template", content: {title: "Foo", body: "Foo123"}},
  {id: "foo1", template: "simple_template", bundled: 2, order: 1, content: {title: "Foo1", body: "Foo123-1"}},
  {id: "foo2", template: "simple_template", bundled: 2, order: 2, content: {title: "Foo2", body: "Foo123-2"}},
  {id: "bar", template: "fancy_template", content: {title: "Foo", body: "Foo123"}},
  {id: "baz", content: {title: "Foo", body: "Foo123"}}
];
export const FAKE_LOCAL_PROVIDER = {id: "onboarding", type: "local", localProvider: "FAKE_LOCAL_PROVIDER", enabled: true, cohort: 0};
export const FAKE_LOCAL_PROVIDERS = {FAKE_LOCAL_PROVIDER: {getMessages: () => FAKE_LOCAL_MESSAGES}};

export const FAKE_REMOTE_MESSAGES = [
  {id: "qux", template: "simple_template", content: {title: "Qux", body: "hello world"}}
];
export const FAKE_REMOTE_PROVIDER = {id: "remotey", type: "remote", url: "http://fake.com/endpoint", enabled: true};

export const FAKE_REMOTE_SETTINGS_PROVIDER = {id: "remotey-settingsy", type: "remote-settings", bucket: "bucketname", enabled: true};

// Stubs methods on RemotePageManager
export class FakeRemotePageManager {
  constructor() {
    this.addMessageListener = sinon.stub();
    this.sendAsyncMessage = sinon.stub();
    this.removeMessageListener = sinon.stub();
    this.browser = {
      ownerGlobal: {
        openTrustedLinkIn: sinon.stub(),
        openLinkIn: sinon.stub(),
        OpenBrowserWindow: sinon.stub()
      }
    };
  }
}
