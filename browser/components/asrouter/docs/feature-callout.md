# Feature Callout

Feature Callouts point to and describe features in content pages or the browser chrome. They can consist of a single message or of a sequence of messages. Callouts are different from Spotlights or other dialogs in that they do not block other interactions with the browser. Feature callouts are currently only available for experimentation in the browser chrome. For example, callouts can easily be configured to point to toolbar buttons in the browser chrome.

Callouts may be configured with the following content elements (each of which is optional):

- title
- subtitle
- inline title icon
- a large illustration above the title
- primary action button
- secondary action button
- additional action button
- dismiss button
- checkboxes and/or radio buttons

The callout's arrow (the triangle-shaped caret pointing to the anchor) can be positioned in the middle or in the corners of any of the callout's edges, and it can be anchored to the same positions on its anchor element. The arrow position and anchor position each support all 8 cardinal and ordinal directions. The arrow can also be hidden entirely. There is also an optional effect to highlight the button the callout is anchored to. This highlight only works if the anchor element is a button.

## Examples

![Feature Callout](./feature-callout.png)

## Testing Feature Callouts

### Via the devtools:

1. Go to `about:config`, set pref `browser.newtabpage.activity-stream.asrouter.devtoolsEnabled` to `true`
2. Open a new tab and go to `about:asrouter` in the urlbar
3. In the devtools Messages section, search for `feature_callout` using the findbar
4. You should see an example JSON message labeled `TEST_FEATURE_TOUR`. Clicking `Show` next to it should show the callout
5. You can directly modify the message in the text area with your changes or by pasting your custom message JSON. Clicking `Modify` shows your updated message. Make sure it's valid JSON and be careful not to add unnecessary commas after the final member in an array or the final property of an object, as they will invalidate the message.
6. For these testing purposes, targeting and trigger are ignored, as the message will be triggered by pressing the "Modify" button. So you won't be able to test triggers and targeting by this method.
6. Ensure that all required properties are covered according to the schema below
7. Clicking `Share` copies a link to your clipboard that can be pasted in the urlbar to preview the message and can be shared to get feedback from your team

- **Note:** Only one Feature Callout can be shown at a time. You must dismiss existing callouts before new ones can be shown.

### Via local provider:

You can also test Feature Callouts by adding them to the [local provider](https://searchfox.org/mozilla-central/source/browser/components/asrouter/modules/FeatureCalloutMessages.sys.mjs). While slower than using the devtools, this is useful when you want to test the trigger or targeting, or when your callout's anchor is an element that is not visible while on `about:asrouter` (such as a urlbar button).

### Via Experiments:

You can test Feature Callouts by creating an experiment or landing message in tree. [Messaging Journey](https://experimenter.info/messaging/desktop-messaging-journey) captures creating and testing experiments via Nimbus. This is the most time-consuming method, but if your callout will be launched as an experiment, then it also provides the most accurate preview.

### Schema

```ts
interface FeatureCallout {
  // Unique id for the message. Used to store impressions, recorded in telemetry
  id: string;
  template: "feature_callout";
  // Targeting expression string. JEXL is used for evaluation. See the Targeting
  // section below for details.
  targeting: string;
  // Trigger object. See the Triggers section below for details.
  trigger: {
    // The trigger's unique identifier, e.g. "nthTabClosed".
    id: string;
    // A set of parameters for the triggers. Usage depends on the trigger id.
    params?: any;
    // A set of URL match patterns (like globs) used by some triggers.
    // See https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns
    patterns?: string[];
  };
  // An optional object specifying frequency caps for the message.
  frequency?: {
    // A basic limit on the number of times the message can be shown to a user
    // across the entire lifetime of the user profile.
    lifetime?: number;
    // An array specifying any number of limits on the number of times the
    // message can be shown to a user within a specific time period. This can be
    // specified in addition to or instead of a lifetime limit.
    custom?: Array<{
      // The number of times the message can be shown within the period.
      cap: number;
      // The period of time in milliseconds. For example, 24 hours is 86400000.
      period: number;
    }>;
  };
  // An array of message groups, which are used for frequency capping. Typically
  // this should be ["cfr"], unless you have a specific reason to do otherwise.
  groups?: string[];
  // Messages can optionally have a priority or weight, influencing the order in
  // which they're shown. Higher priority messages are shown first. Messages can
  // also be selected randomly based on their weight. However, weight is rarely
  // used. We recommend using neither weight nor priority, unless you are adding
  // multiple messages with the same trigger and similar targeting.
  weight?: number; // e.g. weight: 200 is more likely than weight: 100
  priority?: number; // e.g. priority: 2 beats priority: 1
  // Whether the message should be skipped in automated tests. If omitted, the
  // message can be shown in tests. A truthy value will skip the message in
  // tests. The value should be a string explaining why the message needs to be
  // skipped. You can still test messages with this property in automation by
  // stubbing `ASRouter.messagesEnabledInAutomation` (adding the message id to
  // the array). This way, you can avoid showing the message in all tests except
  // the specific test where you want to test it. This has no effect for
  // messages in Nimbus experiments. It's for local messages only.
  skip_in_tests?: string;
  content: {
    // The same as the id above
    id: string;
    template: "multistage";
    backdrop: "transparent";
    transitions: false;
    disableHistoryUpdates: true;
    // The name of a preference that will be used to store screen progress. Only
    // relevant if your callout has multiple screens and serves as a tour. This
    // allows tour progress to persist across sessions and even devices, if the
    // pref is synced via FxA. In most cases, this will not be needed.
    tour_pref_name?: string;
    // A default value for the pref. Can be used if the pref is not set in
    // Firefox's default prefs. This is the default value that will be used
    // until the pref is set by the user interacting with the callout. It will
    // be used to determine the starting screen. Values are JSON objects like
    // this: { "screen": "SCREEN_1", "complete": false }
    // The "screen" property is the id of the screen to start on, and the
    // "complete" property is a boolean indicating whether the tour has been
    // completed (it should always be false here). As with tour_pref_name, this
    // should usually be omitted.
    tour_pref_default_value?: string;
    // Set to "block" to block all telemetry. Recommended to omit this.
    metrics?: string;
    // An array of screens that should be shown in sequence. The first screen
    // will be shown immediately. If the screen includes actions (such as
    // `primary_button.action`) with `navigate: true`, the user can advance to
    // the next screen, causing the first screen to fade out and the next screen
    // to fade in.
    screens: [
      {
        id: string;
        // Feature callouts with multiple screens show a series of dots at the
        // bottom, indicating which screen the user is on. This property allows
        // you to hide those dots. The steps indicator is already hidden if
        // there's only one screen, since it's unnecessary. Defaults to false.
        force_hide_steps_indicator?: boolean;
        // An array of anchor objects. Each anchor object represents a single
        // element on the page that the callout should be anchored to. The
        // callout will be anchored to the first visible element in the array.
        anchors: [
          {
            // A CSS selector for the element to anchor to. The callout will be
            // anchored to the first visible element that matches this selector.
            selector: string;
            // An object representing how the callout should be positioned
            // relative to the anchor element.
            panel_position: {
              // The point on the anchor that the callout should be tied to. See
              // PopupAttachmentPoint below for the possible values. These are
              // the same values used by XULPopupElements.
              anchor_attachment: PopupAttachmentPoint;
              // The point on the callout that should be tied to the anchor.
              callout_attachment: PopupAttachmentPoint;
              // Offsets in pixels to apply to the callout position in the
              // horizontal and vertical directions. Generally not needed.
              offset_x?: number;
              offset_y?: number;
            };
            // Hide the arrow that points from the callout to the anchor?
            hide_arrow?: boolean;
            // Whether to set the [open] style on the anchor element while the
            // callout is showing. False to set it, true to not set it. Not all
            // elements have an [open] style. Buttons do, for example. It's
            // usually similar to :active.
            no_open_on_anchor?: boolean;
            // The desired width of the arrow in a number of pixels. 33.94113 by
            // default (this corresponds to a triangle with 24px edges). This
            // also affects the height of the arrow.
            arrow_width?: number;
          }
        ];
        content: {
          position: "callout";
          // By default, callouts don't hide if the user clicks outside of them.
          // Set this to true to make the callout hide on outside clicks.
          autohide?: boolean;
          // Callout card width as a CSS value, e.g. "400px" or "min-content".
          // Defaults to "400px".
          width?: string;
          // Callout card padding in pixels. Defaults to 24.
          padding?: number;
          // Callouts normally have a vertical layout, with rows of content. If
          // you want a single row with a more inline layout, you can use this
          // property, which works well in tandem with title_logo.
          layout?: "inline";
          // An optional object representing a large illustration to show above
          // other content. See Logo below for the possible properties.
          logo?: Logo;
          // The callout's headline. This is optional but commonly used. Can be
          // a raw string or a LocalizableThing (see interface below).
          title?: Label;
          // An optional object representing an icon to show next to the title.
          // See TitleLogo below for the possible properties.
          title_logo?: TitleLogo;
          // A subtitle to show below the title. Typically a longer paragraph.
          subtitle?: Label;
          primary_button?: {
            // Text to show inside the button.
            label: Label;
            // Buttons can optionally show an arrow icon, indicating that
            // clicking the button will advance to the next screen.
            has_arrow_icon?: boolean;
            // Buttons can be disabled. The boolean option isn't really useful,
            // since there's no logic to enable the button. However, if your
            // screen uses the "multiselect" tile (see tiles), you can use
            // "hasActiveMultiSelect" to disable the button until the user
            // selects something.
            disabled?: boolean | "hasActiveMultiSelect";
            // Primary buttons can have a "primary" or "secondary" style. This
            // is useful because you can't change the order of the buttons, but
            // you can swap the primary and secondary buttons' styles.
            style?: "primary" | "secondary";
            // The action to take when the button is clicked. See Action below.
            action: Action;
          };
          secondary_button?: {
            label: Label;
            // Extra text to show before the button.
            text: Label;
            has_arrow_icon?: boolean;
            disabled?: boolean | "hasActiveMultiSelect";
            style?: "primary" | "secondary";
            action: Action;
          };
          additional_button?: {
            label: Label;
            // If you have several buttons, you can use this property to control
            // the orientation of the buttons. By default, buttons are laid out
            // in a complex way. Use row or column to override this.
            flow?: "row" | "column";
            disabled?: boolean;
            // The additional button can also be styled as a link.
            style?: "primary" | "secondary" | "link";
            action: Action;
            // Justification/alignment of the buttons row/column. Defaults to
            // "end" (right-justified buttons). You can use space-between if,
            // for example, you have 2 buttons and you want one on the left and
            // one on the right.
            alignment?: "start" | "end" | "space-between";
          };
          dismiss_button?: {
            // This can be used to control the ARIA attributes and tooltip.
            // Usually it's omitted, since it has a correct default value.
            label?: Label;
            // The button can be 32px or 24px. Defaults to 32px.
            size?: "small" | "large";
            action: Action;
            // CSS overrides.
            marginBlock?: string;
            marginInline?: string;
          };
          // A split button is an additional_button or secondary_button split
          // into 2 buttons: one that performs the main action, and one with an
          // arrow that opens a dropdown submenu (which this property controls).
          submenu_button?: {
            // This defines the dropdown menu that appears when the user clicks
            // the split button.
            submenu: SubmenuItem[];
            // The submenu button can only be a split button, so a secondary or
            // additional button needs to exist for it to attach to.
            attached_to: "secondary_button" | "additional_button";
            // Used mainly to control the ARIA label and tooltip (tooltips are
            // currently broken), but can also be used to override CSS styles.
            label?: Label;
            // Whether the split button should follow the primary or secondary
            // button style. Set this to the same style you specified for the
            // button it's attached to. Defaults to "secondary".
            style?: "primary" | "secondary";
          };
          // Predefined content modules. The only one currently supported in
          // feature callout is "multiselect", which allows you to show a series
          // of checkboxes and/or radio buttons.
          tiles?: {
            type: "multiselect";
            // Depends on the type, but we only support "multiselect" currently.
            data: MultiSelectItem[];
            // By default, multiselect items appear in the order they're listed
            // in the data array. Set this to true to randomize the order. This
            // is most commonly used to randomize the order of answer choices
            // for a survey question, to avoid the first-choice bias.
            randomize?: boolean;
            // Allows CSS overrides of the multiselect container.
            style?: {
              color?: string;
              fontSize?: string;
              fontWeight?: string;
              letterSpacing?: string;
              lineHeight?: string;
              marginBlock?: string;
              marginInline?: string;
              paddingBlock?: string;
              paddingInline?: string;
              whiteSpace?: string;
              flexDirection?: string;
              flexWrap?: string;
              flexFlow?: string;
              flexGrow?: string;
              flexShrink?: string;
              justifyContent?: string;
              alignItems?: string;
              gap?: string;
              // Any CSS properties starting with "--" are also allowed, to
              // override CSS variables used in _feature-callout.scss.
              "--some-variable"?: string;
            };
          };
          // The dots in the corner that show what screen you're on and how many
          // screens there are in total. This property is only used to override
          // the ARIA attributes or tooltip. Not recommended.
          steps_indicator?: {
            string_id: string;
          };
          // An extra block of configurable content below the title/subtitle but
          // above the optional `tiles` section and the main buttons. Styles not
          // yet implemented; not recommended.
          above_button_content?: LinkParagraphOrImage[];
          // An optional array of event listeners to add to the page where the
          // feature callout is shown. This can be used to perform actions in
          // response to interactions and other events outside of the feature
          // callout itself. The prototypical use case is dismissing the feature
          // callout when the user clicks the button the callout is anchored to.
          // It also supports performing actions on a timeout/interval.
          page_event_listeners?: Array<{
            params: {
              // Event type string, e.g. "click". This supports:
              // 1. Any DOM event type
              // 2. "timeout" and "interval" for timers
              // 3. Internal feature callout events: "touradvance" and
              //    "tourend". This can be used to perform actions when the user
              //    advances to the next screen or finishes the callout tour.
              type: string;
              // Target selector, e.g. `tag.class, #id[attr]` - Not needed for
              // all types.
              selectors?: string;
              // addEventListener options
              options: {
                // Handle events in capturing phase?
                capture?: boolean;
                // Remove listener after first event?
                once?: boolean;
                // Prevent default action in event handler?
                preventDefault?: boolean;
                // Used only for `timeout` and `interval` event types. These
                // don't set up real event listeners, but instead invoke the
                // action on a timer.
                interval?: number;
              };
            };
            action: {
              // One of the special message action ids.
              type?: "string";
              // Data to pass to the action. Depends on the action.
              data?: any;
              // Dismiss screen after performing action? If there's no type, the
              // action will *only_ dismiss the callout.
              dismiss?: boolean;
            };
          }>;
        };
      }
    ];
    // Specify the index of the screen to start on. Generally unused.
    startScreen?: number;
  };
}

type PopupAttachmentPoint =
  | "topleft"
  | "topright"
  | "bottomleft"
  | "bottomright"
  | "leftcenter"
  | "rightcenter"
  | "topcenter"
  | "bottomcenter";

type Label = string | LocalizableThing;

interface LocalizableThing {
  // A raw, untranslated string, typically used for EN-only experiments.
  raw?: string;
  // A Fluent string id from a .ftl file.
  string_id?: string;
  // Arguments to pass to Fluent. Used for Fluent strings that have variables.
  args?: {
    [key: string]: string;
  };
  // A string to use as the element's aria-label attribute value.
  aria_label?: string;
  // CSS overrides.
  color?: string;
  fontSize?: string;
  fontWeight?: string;
  letterSpacing?: string;
  lineHeight?: string;
  marginBlock?: string;
  marginInline?: string;
  paddingBlock?: string;
  paddingInline?: string;
  whiteSpace?: string;
}

interface Logo {
  imageURL: "chrome://branding/content/about-logo.svg";
  darkModeImageURL: string;
  reducedMotionImageURL: string;
  darkModeReducedMotionImageURL: string;
  // <img> alt text. Defaults to ""
  alt: string;
  // CSS style overrides for the icon.
  width: string;
  height: string;
  marginBlock: string;
  marginInline: string;
}

interface TitleLogo extends Logo {
  // Logo alignment relative to the title. Use "top" if you have multiple rows
  // of text and you want the logo aligned to the top row. Defaults to "center".
  alignment: "top" | "bottom" | "center";
}

// Click the Special Message Actions link at the bottom of the page.
interface Action {
  // One of the special message action ids.
  type?: "string";
  // Data to pass to the action. Depends on the action.
  data?: any;
  // Set to true if you want the action to advance to the next screen or hide
  // the callout if it's the last screen. Can be used in lieu of "type" and
  // "data" to create a button that just advances the screen.
  navigate?: boolean;
  // Same as "navigate" but dismisses the callout instead of advancing to the
  // next screen.
  dismiss?: boolean;
  // Set to true if this action is for the primary button and you're using the
  // "multiselect" tile. This is what allows the primary button to perform the
  // actions specified by the user's checkbox/radio selections. It will combine
  // all the actions for all the selected checkboxes/radios into this action's
  // data.actions array, and perform them in series.
  collectSelect?: boolean;
}

// Either an image or a paragraph that supports inline links. Currently requires
// Fluent strings. Raw strings are not supported.
interface LinkParagraphOrImage extends Logo {
  // Which type of content this is.
  type: "image" | "text";

  // Each of these is only used if `type` is "text".
  // The `text` object contains the Fluent string id. Doesn't support raw text.
  text: LocalizableThing;
  // An array of key names. Each link key must exist in screen.content. For
  // example, if link_keys is ["learn_more"], then there must be a key named
  // "learn_more" in screen.content. The value of that key must be an object
  // with an `action` property (which is an Action). Moreover, the string_id in
  // the `text` object (see the property above) must refer to a Fluent string
  // that contains an anchor element with `data-l10n-name="learn_more"`, e.g.:
  //   my-string = Do the thing! <a data-l10n-name="learn_more">Learn more</a>
  link_keys: string[];
}

interface MultiSelectItem {
  // A unique id for this item, distinguishing it from other items.
  id: string;
  type: "checkbox" | "radio";
  // Radios need to be members of radio groups to work properly. Set the same
  // group for your radios to make sure only one can be selected at a time.
  group?: string;
  // Set to true to make it selected/checked by default.
  defaultValue: false;
  label?: Label;
  // CSS overrides for the div box containing the item and its optional label.
  style?: {
    color?: string;
    fontSize?: string;
    fontWeight?: string;
    letterSpacing?: string;
    lineHeight?: string;
    marginBlock?: string;
    marginInline?: string;
    paddingBlock?: string;
    paddingInline?: string;
    whiteSpace?: string;
    flexDirection?: string;
    flexWrap?: string;
    flexFlow?: string;
    flexGrow?: string;
    flexShrink?: string;
    justifyContent?: string;
    alignItems?: string;
    gap?: string;
  };
  // You can replace the checkbox check/radio circle with an icon by using a
  // bunch of CSS overrides.
  icon?: {
    style: {
      color?: string;
      fontSize?: string;
      fontWeight?: string;
      letterSpacing?: string;
      lineHeight?: string;
      marginBlock?: string;
      marginInline?: string;
      paddingBlock?: string;
      paddingInline?: string;
      whiteSpace?: string;
      width?: string;
      height?: string;
      background?: string;
      backgroundColor?: string;
      backgroundImage?: string;
      backgroundSize?: string;
      backgroundPosition?: string;
      backgroundRepeat?: string;
      backgroundOrigin?: string;
      backgroundClip?: string;
      border?: string;
      borderRadius?: string;
      appearance?: string;
      fill?: string;
      stroke?: string;
      outline?: string;
      outlineOffset?: string;
      boxShadow?: string;
    };
  };
  // The action is not performed until the user clicks the primary button.
  action: Action;
}

interface SubmenuItem {
  // Submenus can have 3 types of items, just like normal menupopups
  // in Firefox.
  type: "action" | "menu" | "separator";
  // The id is used to identify the submenu item in telemetry.
  id?: string;
  label: Label;
  // Used only for type "action". The action to perform when the
  // submenu item is clicked.
  action?: Action;
  // Used only for type "menu". The submenu items to show when the
  // user hovers over this item. This is a recursive structure.
  submenu: SubmenuItem[];
  // An optional URL specifying an icon to show next to the label.
  icon?: string;
}
```

### Example JSON

```json
{
  "id": "TEST_FEATURE_TOUR",
  "template": "feature_callout",
  "groups": [],
  "targeting": "true",
  "content": {
    "id": "TEST_FEATURE_TOUR",
    "template": "multistage",
    "backdrop": "transparent",
    "transitions": false,
    "disableHistoryUpdates": true,
    "screens": [
      {
        "id": "FEATURE_CALLOUT_1",
        "anchors": [
          {
            "selector": "#PanelUI-menu-button",
            "panel_position": {
              "anchor_attachment": "bottomcenter",
              "callout_attachment": "topright"
            }
          }
        ],
        "content": {
          "position": "callout",
          "title": {
            "raw": "Panel Feature Callout"
          },
          "subtitle": {
            "raw": "Hello!"
          },
          "primary_button": {
            "label": {
              "raw": "Advance"
            },
            "action": {
              "navigate": true
            }
          },
          "dismiss_button": {
            "action": {
              "dismiss": true
            }
          }
        }
      }
    ]
  },
}
```

### Targeting

Messages use JEXL targeting expressions to determine whether the user is eligible to see the message. See [Guide to targeting with JEXL](./targeting-guide.md) and [Targeting attributes](./targeting-attributes.md) for details.

### Triggers

Triggers are used to determine when a message should be shown. See [Trigger Listeners](/toolkit/components/messaging-system/docs/TriggerActionSchemas/index.md) for details.

### Special Message Actions

Buttons, links, and other calls to action can use one or more of a set of predefined actions. [Click here](/toolkit/components/messaging-system/docs/SpecialMessageActionSchemas/index.md) for a full list of valid actions.
