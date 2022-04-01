/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
import panelMessaging from "../../messages";

function TagPicker(props) {
  const [tags, setTags] = useState(props.tags);
  const [duplicateTag, setDuplicateTag] = useState(null);
  const [inputValue, setInputValue] = useState("");
  const [usedTags, setUsedTags] = useState([]);
  // Status be success, waiting, or error.
  const [
    { tagInputStatus, tagInputErrorMessage },
    setTagInputStatus,
  ] = useState({
    tagInputStatus: "",
    tagInputErrorMessage: "",
  });

  const inputToSubmit = inputValue.trim();
  const tagsToSubmit = [...tags, ...(inputToSubmit ? [inputToSubmit] : [])];

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
        addTag();
      } else if (enterKey) {
        submitTags();
      }
    }
  };

  let addTag = () => {
    let newDuplicateTag = tags.find(item => item === inputToSubmit);

    if (!inputToSubmit?.length) {
      return;
    }

    setInputValue(``); // Clear out input
    if (!newDuplicateTag) {
      setTags(tagsToSubmit);
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
      function(resp) {
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
    panelMessaging.sendMessage("PKT_getTags", {}, resp =>
      setUsedTags(resp?.data?.tags)
    );
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
                {tag}
                <button
                  onClick={() => removeTag(i)}
                  className={`stp_tag_picker_tag_remove`}
                >
                  X
                </button>
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
                {usedTags.map(item => (
                  <option key={item} value={item} />
                ))}
              </datalist>
              <button
                className="stp_tag_picker_button"
                disabled={!tagsToSubmit?.length}
                data-l10n-id="pocket-panel-saved-save-tags"
                onClick={() => submitTags()}
              />
            </div>
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
