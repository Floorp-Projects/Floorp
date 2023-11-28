import React from "react";
import { mount } from "enzyme";
import { OnboardingVideo } from "content-src/components/OnboardingVideo";

describe("OnboardingVideo component", () => {
  let sandbox;

  beforeEach(async () => {
    sandbox = sinon.createSandbox();
  });

  afterEach(() => {
    sandbox.restore();
  });

  const SCREEN_PROPS = {
    content: {
      title: "Test title",
      video_container: {
        video_url: "test url",
      },
    },
  };

  it("should handle video_start action when video is played", () => {
    const handleAction = sandbox.stub();
    const wrapper = mount(
      <OnboardingVideo handleAction={handleAction} {...SCREEN_PROPS} />
    );
    wrapper.find("video").simulate("play");
    assert.calledWith(handleAction, {
      currentTarget: { value: "video_start" },
    });
  });
  it("should handle video_end action when video has completed playing", () => {
    const handleAction = sandbox.stub();
    const wrapper = mount(
      <OnboardingVideo handleAction={handleAction} {...SCREEN_PROPS} />
    );
    wrapper.find("video").simulate("ended");
    assert.calledWith(handleAction, {
      currentTarget: { value: "video_end" },
    });
  });
});
