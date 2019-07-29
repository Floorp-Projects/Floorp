export const CHILD_TO_PARENT_MESSAGE_NAME = "ASRouter:child-to-parent";
export const PARENT_TO_CHILD_MESSAGE_NAME = "ASRouter:parent-to-child";

export const FAKE_LOCAL_MESSAGES = [
  {
    id: "foo",
    provider: "snippets",
    template: "simple_snippet",
    content: { title: "Foo", body: "Foo123" },
  },
  {
    id: "foo1",
    template: "simple_snippet",
    provider: "snippets",
    bundled: 2,
    order: 1,
    content: { title: "Foo1", body: "Foo123-1" },
  },
  {
    id: "foo2",
    template: "simple_snippet",
    provider: "snippets",
    bundled: 2,
    order: 2,
    content: { title: "Foo2", body: "Foo123-2" },
  },
  {
    id: "bar",
    template: "fancy_template",
    content: { title: "Foo", body: "Foo123" },
  },
  { id: "baz", content: { title: "Foo", body: "Foo123" } },
  {
    id: "newsletter",
    provider: "snippets",
    template: "newsletter_snippet",
    content: { title: "Foo", body: "Foo123" },
  },
  {
    id: "fxa",
    provider: "snippets",
    template: "fxa_signup_snippet",
    content: { title: "Foo", body: "Foo123" },
  },
  {
    id: "belowsearch",
    provider: "snippets",
    template: "simple_below_search_snippet",
    content: { text: "Foo" },
  },
];
export const FAKE_LOCAL_PROVIDER = {
  id: "onboarding",
  type: "local",
  localProvider: "FAKE_LOCAL_PROVIDER",
  enabled: true,
  cohort: 0,
};
export const FAKE_LOCAL_PROVIDERS = {
  FAKE_LOCAL_PROVIDER: { getMessages: () => FAKE_LOCAL_MESSAGES },
};

export const FAKE_REMOTE_MESSAGES = [
  {
    id: "qux",
    template: "simple_snippet",
    content: { title: "Qux", body: "hello world" },
  },
];
export const FAKE_REMOTE_PROVIDER = {
  id: "remotey",
  type: "remote",
  url: "http://fake.com/endpoint",
  enabled: true,
};

export const FAKE_REMOTE_SETTINGS_PROVIDER = {
  id: "remotey-settingsy",
  type: "remote-settings",
  bucket: "bucketname",
  enabled: true,
};

export const FAKE_RECOMMENDATION = {
  id: "fake_id",
  template: "cfr_doorhanger",
  content: {
    category: "cfrDummy",
    bucket_id: "fake_bucket_id",
    notification_text: "Fake Notification Text",
    info_icon: {
      label: "Fake Info Icon Label",
      sumo_path: "a_help_path_fragment",
    },
    heading_text: "Fake Heading Text",
    addon: {
      title: "Fake Addon Title",
      author: "Fake Addon Author",
      icon: "a_path_to_some_icon",
      rating: 4.2,
      users: 1234,
      amo_url: "a_path_to_amo",
    },
    descriptionDetails: {
      steps: [{ string_id: "cfr-features-step1" }],
    },
    text: "Here is the recommendation text body",
    buttons: {
      primary: {
        label: { string_id: "primary_button_id" },
        action: {
          id: "primary_action",
          data: {},
        },
      },
      secondary: [
        {
          label: { string_id: "secondary_button_id" },
          action: { id: "secondary_action" },
        },
        {
          label: { string_id: "secondary_button_id_2" },
          action: { id: "secondary_action" },
        },
        {
          label: { string_id: "secondary_button_id_3" },
          action: { id: "secondary_action" },
        },
      ],
    },
  },
};

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
        OpenBrowserWindow: sinon.stub(),
        openPreferences: sinon.stub(),
        gBrowser: {
          pinTab: sinon.stub(),
          selectedTab: {},
        },
        ConfirmationHint: {
          show: sinon.stub(),
        },
      },
    };
    this.portID = "6000:2";
  }
}
