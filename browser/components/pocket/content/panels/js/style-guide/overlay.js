import React from "react";
import ReactDOM from "react-dom";
import ArticleList from "../components/ArticleList/ArticleList";
import PopularTopics from "../components/PopularTopics/PopularTopics";

var StyleGuideOverlay = function(options) {};

StyleGuideOverlay.prototype = {
  create() {
    // TODO: Wrap popular topics component in JSX to work without needing an explicit container hierarchy for styling
    ReactDOM.render(
      <div>
        <h3>JSX Components:</h3>
        <h4 className="stp_styleguide_h4">PopularTopics</h4>
        <PopularTopics
          pockethost={`getpocket.com`}
          utmsource={`styleguide`}
          topics={[
            { title: "Self Improvement", topic: "self-improvement" },
            { title: "Food", topic: "food" },
            { title: "Entertainment", topic: "entertainment" },
            { title: "Science", topic: "science" },
          ]}
        />
        <h4 className="stp_styleguide_h4">ArticleList</h4>
        <ArticleList
          articles={[
            {
              title: "Article Title",
              publisher: "Publisher",
              thumbnail:
                "https://img-getpocket.cdn.mozilla.net/80x80/https://www.raritanheadwaters.org/wp-content/uploads/2020/04/red-fox.jpg",
              url: "https://example.org",
              alt: "Alt Text",
            },
            {
              title: "Article Title",
              publisher: "Publisher",
              thumbnail:
                "https://img-getpocket.cdn.mozilla.net/80x80/https://www.raritanheadwaters.org/wp-content/uploads/2020/04/red-fox.jpg",
              url: "https://example.org",
              alt: "Alt Text",
            },
            {
              title: "Article Title",
              publisher: "Publisher",
              thumbnail:
                "https://img-getpocket.cdn.mozilla.net/80x80/https://www.raritanheadwaters.org/wp-content/uploads/2020/04/red-fox.jpg",
              url: "https://example.org",
              alt: "Alt Text",
            },
          ]}
        />
      </div>,
      document.querySelector(`#stp_style_guide_components`)
    );
  },
};

export default StyleGuideOverlay;
