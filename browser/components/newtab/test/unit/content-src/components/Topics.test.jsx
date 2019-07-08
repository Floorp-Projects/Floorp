import { Topic, Topics } from "content-src/components/Topics/Topics";
import React from "react";
import { shallow } from "enzyme";

describe("<Topics>", () => {
  it("should render a Topics element", () => {
    const wrapper = shallow(<Topics topics={[]} />);
    assert.ok(wrapper.exists());
  });
  it("should render a Topic element for each topic with the right url", () => {
    const data = [
      { name: "topic1", url: "https://topic1.com" },
      { name: "topic2", url: "https://topic2.com" },
    ];

    const wrapper = shallow(<Topics topics={data} />);

    const topics = wrapper.find(Topic);
    assert.lengthOf(topics, 2);
    topics.forEach((topic, i) => assert.equal(topic.props().url, data[i].url));
  });
});
