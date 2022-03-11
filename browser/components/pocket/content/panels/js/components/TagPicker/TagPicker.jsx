/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState } from "react";

function TagPicker(props) {
  const [tags, setTags] = useState(props.tags);
  const [duplicateTag, setDuplicateTag] = useState(null);

  let handleKeyDown = e => {
    // Enter tag on comma or enter keypress
    if (e.keyCode === 188 || e.keyCode === 13) {
      let tag = e.target.value.trim();

      e.preventDefault();
      e.target.value = ``; // Clear out input

      addTag(tag);
    }
  };

  let addTag = tag => {
    let newDuplicateTag = tags.find(item => item === tag);

    if (!tag.length) {
      return;
    }

    if (!newDuplicateTag) {
      setTags([...tags, tag]);
    } else {
      setDuplicateTag(newDuplicateTag);

      setTimeout(() => {
        setDuplicateTag(null);
      }, 1000);
    }
  };

  let removeTag = index => {
    let updatedTags = tags.slice(0); // Shallow copied array
    updatedTags.splice(index, 1);
    setTags(updatedTags);
  };

  return (
    <div className="stp_tag_picker">
      <p>Add Tags:</p>
      <div className="stp_tag_picker_tags">
        {tags.map((tag, i) => (
          <div
            className={`stp_tag_picker_tag${
              duplicateTag === tag ? ` stp_tag_picker_tag_duplicate` : ``
            }`}
          >
            {tag}
            <button
              onClick={() => removeTag(i)}
              className={`stp_tag_picker_tag_remove`}
            >
              X
            </button>
          </div>
        ))}
        <input
          className="stp_tag_picker_input"
          type="text"
          onKeyDown={e => handleKeyDown(e)}
          maxlength="25"
        ></input>
      </div>
    </div>
  );
}

export default TagPicker;
