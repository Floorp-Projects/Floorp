import React from "react";
import ReactDOM from "react-dom";
import PopularTopics from "../components/PopularTopics";

var StyleGuideOverlay = function(options) {};

StyleGuideOverlay.prototype = {
  create() {
    // TODO: Wrap popular topics component in JSX to work without needing an explicit container hierarchy for styling
    ReactDOM.render(
      <PopularTopics
        pockethost={`getpocket.com`}
        utmsource={`styleguide`}
        topics={[
          { title: "Self Improvement", topic: "self-improvement" },
          { title: "Food", topic: "food" },
          { title: "Entertainment", topic: "entertainment" },
          { title: "Science", topic: "science" },
        ]}
      />,
      document.querySelector(`#stp_style_guide_components`)
    );
  },
};

export default StyleGuideOverlay;
