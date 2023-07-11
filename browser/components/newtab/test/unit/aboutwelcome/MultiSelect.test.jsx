import React from "react";
import { mount } from "enzyme";
import { MultiSelect } from "content-src/aboutwelcome/components/MultiSelect";

describe("Multistage AboutWelcome module", () => {
  let sandbox;
  let MULTISELECT_SCREEN_PROPS;
  let setActiveMultiSelect;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    setActiveMultiSelect = sandbox.stub();
    MULTISELECT_SCREEN_PROPS = {
      id: "multiselect-screen",
      content: {
        position: "split",
        split_narrow_bkg_position: "-60px",
        image_alt_text: {
          string_id: "mr2022-onboarding-default-image-alt",
        },
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-settodefault.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: "Test Title",
        subtitle: "Test SubTitle",
        tiles: {
          type: "multiselect",
          data: [
            {
              id: "checkbox-1",
              defaultValue: true,
              label: {
                string_id: "mr2022-onboarding-set-default-primary-button-label",
              },
              action: {
                type: "SET_DEFAULT_BROWSER",
              },
            },
            {
              id: "checkbox-2",
              defaultValue: true,
              label: "Test Checkbox 2",
              action: {
                type: "SHOW_MIGRATION_WIZARD",
                data: {},
              },
            },
            {
              id: "checkbox-3",
              defaultValue: false,
              label: "Test Checkbox 3",
              action: {
                type: "SHOW_MIGRATION_WIZARD",
                data: {},
              },
            },
          ],
        },
        primary_button: {
          label: "Save and Continue",
          action: {
            type: "MULTI_ACTION",
            collectSelect: true,
            navigate: true,
            data: { actions: [] },
          },
        },
        secondary_button: {
          label: "Skip",
          action: {
            navigate: true,
          },
          has_arrow_icon: true,
        },
      },
    };
  });
  afterEach(() => {
    sandbox.restore();
  });

  describe("MultiSelect component", () => {
    it("should call setActiveMultiSelect with ids of checkboxes with defaultValue true", () => {
      const wrapper = mount(
        <MultiSelect
          setActiveMultiSelect={setActiveMultiSelect}
          {...MULTISELECT_SCREEN_PROPS}
        />
      );

      wrapper.setProps({ activeMultiSelect: null });
      assert.calledOnce(setActiveMultiSelect);
      assert.calledWith(setActiveMultiSelect, ["checkbox-1", "checkbox-2"]);
    });

    it("should use activeMultiSelect ids to set checked state for respective checkbox", () => {
      const wrapper = mount(
        <MultiSelect
          setActiveMultiSelect={setActiveMultiSelect}
          {...MULTISELECT_SCREEN_PROPS}
        />
      );

      wrapper.setProps({ activeMultiSelect: ["checkbox-1", "checkbox-2"] });
      const checkBoxes = wrapper.find(".checkbox-container input");
      assert.strictEqual(checkBoxes.length, 3);

      assert.strictEqual(checkBoxes.first().props().checked, true);
      assert.strictEqual(checkBoxes.at(1).props().checked, true);
      assert.strictEqual(checkBoxes.last().props().checked, false);
    });

    it("should filter out id when checkbox is unchecked", () => {
      const wrapper = mount(
        <MultiSelect
          setActiveMultiSelect={setActiveMultiSelect}
          {...MULTISELECT_SCREEN_PROPS}
        />
      );
      wrapper.setProps({ activeMultiSelect: ["checkbox-1", "checkbox-2"] });

      const ckbx1 = wrapper.find(".checkbox-container input").at(0);
      assert.strictEqual(ckbx1.prop("value"), "checkbox-1");
      ckbx1.getDOMNode().checked = false;
      ckbx1.simulate("change");
      assert.calledWith(setActiveMultiSelect, ["checkbox-2"]);
    });

    it("should add id when checkbox is checked", () => {
      const wrapper = mount(
        <MultiSelect
          setActiveMultiSelect={setActiveMultiSelect}
          {...MULTISELECT_SCREEN_PROPS}
        />
      );
      wrapper.setProps({ activeMultiSelect: ["checkbox-1", "checkbox-2"] });

      const ckbx3 = wrapper.find(".checkbox-container input").at(2);
      assert.strictEqual(ckbx3.prop("value"), "checkbox-3");
      ckbx3.getDOMNode().checked = true;
      ckbx3.simulate("change");
      assert.calledWith(setActiveMultiSelect, [
        "checkbox-1",
        "checkbox-2",
        "checkbox-3",
      ]);
    });

    it("should render radios and checkboxes with correct styles", async () => {
      const SCREEN_PROPS = { ...MULTISELECT_SCREEN_PROPS };
      SCREEN_PROPS.content.tiles.style = { flexDirection: "row", gap: "24px" };
      SCREEN_PROPS.content.tiles.data = [
        {
          id: "checkbox-1",
          defaultValue: true,
          label: { raw: "Test1" },
          action: { type: "OPEN_PROTECTION_REPORT" },
          style: { color: "red" },
          icon: { style: { color: "blue" } },
        },
        {
          id: "radio-1",
          type: "radio",
          group: "radios",
          defaultValue: true,
          label: { raw: "Test3" },
          action: { type: "OPEN_PROTECTION_REPORT" },
          style: { color: "purple" },
          icon: { style: { color: "yellow" } },
        },
      ];
      const wrapper = mount(
        <MultiSelect
          setActiveMultiSelect={setActiveMultiSelect}
          {...SCREEN_PROPS}
        />
      );

      // wait for effect hook
      await new Promise(resolve => queueMicrotask(resolve));
      // activeMultiSelect was called on effect hook with default values
      assert.calledWith(setActiveMultiSelect, ["checkbox-1", "radio-1"]);

      const container = wrapper.find(".multi-select-container");
      assert.strictEqual(container.prop("style").flexDirection, "row");
      assert.strictEqual(container.prop("style").gap, "24px");

      // checkboxes/radios are rendered with correct styles
      const checkBoxes = wrapper.find(".checkbox-container");
      assert.strictEqual(checkBoxes.length, 2);
      assert.strictEqual(checkBoxes.first().prop("style").color, "red");
      assert.strictEqual(checkBoxes.at(1).prop("style").color, "purple");

      const checks = wrapper.find(".checkbox-container input");
      assert.strictEqual(checks.length, 2);
      assert.strictEqual(checks.first().prop("style").color, "blue");
      assert.strictEqual(checks.at(1).prop("style").color, "yellow");
    });
  });
});
