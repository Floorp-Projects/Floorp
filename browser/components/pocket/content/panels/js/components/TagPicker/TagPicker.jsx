/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
import panelMessaging from "../../messages";

function TagPicker(props) {
  const [tags, setTags] = useState(props.tags); // New tag group to store
  const [allTags, setAllTags] = useState([]); // All tags ever used (in no particular order)
  const [recentTags, setRecentTags] = useState([]); // Most recently used tags
  const [duplicateTag, setDuplicateTag] = useState(null);
  const [inputValue, setInputValue] = useState("");

  // Status can be success, waiting, or error.
  const [{ tagInputStatus, tagInputErrorMessage }, setTagInputStatus] =
    useState({
      tagInputStatus: "",
      tagInputErrorMessage: "",
    });

  let handleKeyDown = e => {
    const enterKey = e.keyCode === 13;
    const commaKey = e.keyCode === 188;
    const tabKey = inputValue && e.keyCode === 9;

    // Submit tags on enter with no input.
    // Enter tag on comma, tab, or enter with input.
    // Tab to next element with no input.
    if (commaKey || enterKey || tabKey) {
      e.preventDefault();
      if (inputValue) {
        addTag(inputValue.trim());
        setInputValue(``); // Clear out input
      } else if (enterKey) {
        submitTags();
      }
    }
  };

  let addTag = tagToAdd => {
    if (!tagToAdd?.length) {
      return;
    }

    let newDuplicateTag = tags.find(item => item === tagToAdd);

    if (!newDuplicateTag) {
      setTags([...tags, tagToAdd]);
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

  let submitTags = () => {
    let tagsToSubmit = [];

    if (tags?.length) {
      tagsToSubmit = tags;
    }

    // Capture tags that have been typed in but not explicitly added to the tag collection
    if (inputValue?.trim().length) {
      tagsToSubmit.push(inputValue.trim());
    }

    if (!props.itemUrl || !tagsToSubmit?.length) {
      return;
    }

    setTagInputStatus({
      tagInputStatus: "waiting",
      tagInputErrorMessage: "",
    });
    panelMessaging.sendMessage(
      "PKT_addTags",
      {
        url: props.itemUrl,
        tags: tagsToSubmit,
      },
      function (resp) {
        const { data } = resp;

        if (data.status === "success") {
          setTagInputStatus({
            tagInputStatus: "success",
            tagInputErrorMessage: "",
          });
        } else if (data.status === "error") {
          setTagInputStatus({
            tagInputStatus: "error",
            tagInputErrorMessage: data.error.message,
          });
        }
      }
    );
  };

  useEffect(() => {
    panelMessaging.sendMessage("PKT_getTags", {}, resp => {
      setAllTags(resp?.data?.tags);
    });
  }, []);

  useEffect(() => {
    panelMessaging.sendMessage("PKT_getRecentTags", {}, resp => {
      setRecentTags(resp?.data?.recentTags);
    });
  }, []);

  return (
    <div className="stp_tag_picker">
      {!tagInputStatus && (
        <>
          <h3
            className="header_small"
            data-l10n-id="pocket-panel-signup-add-tags"
          />
          <div className="stp_tag_picker_tags">
            {tags.map((tag, i) => (
              <div
                className={`stp_tag_picker_tag${
                  duplicateTag === tag ? ` stp_tag_picker_tag_duplicate` : ``
                }`}
              >
                <button
                  onClick={() => removeTag(i)}
                  className={`stp_tag_picker_tag_remove`}
                >
                  X
                </button>
                {tag}
              </div>
            ))}
            <div className="stp_tag_picker_input_wrapper">
              <input
                className="stp_tag_picker_input"
                type="text"
                list="tag-list"
                value={inputValue}
                onChange={e => setInputValue(e.target.value)}
                onKeyDown={e => handleKeyDown(e)}
                maxlength="25"
              />
              <datalist id="tag-list">
                {allTags
                  .sort((a, b) => a.search(inputValue) - b.search(inputValue))
                  .map(item => (
                    <option key={item} value={item} />
                  ))}
              </datalist>
              <button
                className="stp_tag_picker_button"
                disabled={!inputValue?.length && !tags.length}
                data-l10n-id="pocket-panel-saved-save-tags"
                onClick={() => submitTags()}
              />
            </div>
          </div>
          <div className="recent_tags">
            {recentTags
              .slice(0, 3)
              .filter(recentTag => {
                return !tags.find(item => item === recentTag);
              })
              .map(tag => (
                <div className="stp_tag_picker_tag">
                  <button
                    className="stp_tag_picker_tag_remove"
                    onClick={() => addTag(tag)}
                  >
                    + {tag}
                  </button>
                </div>
              ))}
          </div>
        </>
      )}
      {tagInputStatus === "waiting" && (
        <h3
          className="header_large"
          data-l10n-id="pocket-panel-saved-processing-tags"
        />
      )}
      {tagInputStatus === "success" && (
        <h3
          className="header_large"
          data-l10n-id="pocket-panel-saved-tags-saved"
        />
      )}
      {tagInputStatus === "error" && (
        <h3 className="header_small">{tagInputErrorMessage}</h3>
      )}
    </div>
  );
}

export default TagPicker;
