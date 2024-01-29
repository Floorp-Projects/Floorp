export const CHILD_TO_PARENT_MESSAGE_NAME = "ASRouter:child-to-parent";
export const PARENT_TO_CHILD_MESSAGE_NAME = "ASRouter:parent-to-child";

export const FAKE_LOCAL_MESSAGES = [
  {
    id: "foo",
    template: "milestone_message",
    content: { title: "Foo", body: "Foo123" },
  },
  {
    id: "foo1",
    template: "fancy_template",
    bundled: 2,
    order: 1,
    content: { title: "Foo1", body: "Foo123-1" },
  },
  {
    id: "foo2",
    template: "fancy_template",
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
    template: "fancy_template",
    content: { title: "Foo", body: "Foo123" },
  },
  {
    id: "fxa",
    template: "fancy_template",
    content: { title: "Foo", body: "Foo123" },
  },
  {
    id: "belowsearch",
    template: "fancy_template",
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
  FAKE_LOCAL_PROVIDER: {
    getMessages: () => Promise.resolve(FAKE_LOCAL_MESSAGES),
  },
};

export const FAKE_REMOTE_MESSAGES = [
  {
    id: "qux",
    template: "fancy_template",
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
  collection: "collectionname",
  enabled: true,
};

const notificationText = new String("Fake notification text"); // eslint-disable-line
notificationText.attributes = { tooltiptext: "Fake tooltip text" };

export const FAKE_RECOMMENDATION = {
  id: "fake_id",
  template: "cfr_doorhanger",
  content: {
    category: "cfrDummy",
    bucket_id: "fake_bucket_id",
    notification_text: notificationText,
    info_icon: {
      label: "Fake Info Icon Label",
      sumo_path: "a_help_path_fragment",
    },
    heading_text: "Fake Heading Text",
    icon_class: "Fake Icon class",
    addon: {
      title: "Fake Addon Title",
      author: "Fake Addon Author",
      icon: "a_path_to_some_icon",
      rating: "4.2",
      users: "1234",
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
        },
        {
          label: { string_id: "secondary_button_id_3" },
          action: { id: "secondary_action" },
        },
      ],
    },
  },
};
