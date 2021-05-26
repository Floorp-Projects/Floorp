/******/ (() => { // webpackBootstrap
/******/ 	"use strict";
/******/ 	var __webpack_modules__ = ({

/***/ 9023:
/*!************************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/DomTranslationManager.ts ***!
  \************************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.DomTranslationManager = void 0;
const TranslationDocument_1 = __webpack_require__(/*! ./TranslationDocument */ 992);
const BergamotDomTranslator_1 = __webpack_require__(/*! ./dom-translators/BergamotDomTranslator */ 1518);
const getTranslationNodes_1 = __webpack_require__(/*! ./getTranslationNodes */ 5173);
const ContentScriptLanguageDetectorProxy_1 = __webpack_require__(/*! ../../shared-resources/ContentScriptLanguageDetectorProxy */ 6336);
const BaseTranslationState_1 = __webpack_require__(/*! ../../shared-resources/models/BaseTranslationState */ 4779);
const LanguageSupport_1 = __webpack_require__(/*! ../../shared-resources/LanguageSupport */ 5602);
const detagAndProject_1 = __webpack_require__(/*! ./dom-translators/detagAndProject */ 961);
class DomTranslationManager {
    constructor(documentTranslationStateCommunicator, document, contentWindow) {
        this.documentTranslationStateCommunicator = documentTranslationStateCommunicator;
        this.document = document;
        this.contentWindow = contentWindow;
        this.languageDetector = new ContentScriptLanguageDetectorProxy_1.ContentScriptLanguageDetectorProxy();
    }
    attemptToDetectLanguage() {
        return __awaiter(this, void 0, void 0, function* () {
            console.debug("Attempting to detect language");
            const url = String(this.document.location);
            if (!url.startsWith("http://") && !url.startsWith("https://")) {
                console.debug("Not a HTTP(S) url, translation unavailable", { url });
                this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.UNAVAILABLE);
                return;
            }
            console.debug("Setting status to reflect detection of language ongoing");
            this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.DETECTING_LANGUAGE);
            // Grab a 60k sample of text from the page.
            // (The CLD2 library used by the language detector is capable of
            // analyzing raw HTML. Unfortunately, that takes much more memory,
            // and since it's hosted by emscripten, and therefore can't shrink
            // its heap after it's grown, it has a performance cost.
            // So we send plain text instead.)
            const translationNodes = getTranslationNodes_1.getTranslationNodes(document.body);
            const domElementsToStringWithMaxLength = (elements, maxLength) => {
                return elements
                    .map(el => el.textContent)
                    .join("\n")
                    .substr(0, maxLength);
            };
            const string = domElementsToStringWithMaxLength(translationNodes.map(tn => tn.content), 60 * 1024);
            // Language detection isn't reliable on very short strings.
            if (string.length < 100) {
                console.debug("Language detection isn't reliable on very short strings. Skipping language detection", { string });
                this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.LANGUAGE_NOT_DETECTED);
                return;
            }
            const detectedLanguageResults = yield this.languageDetector.detectLanguage(string);
            console.debug("Language detection results are in", {
                detectedLanguageResults,
            });
            // The window might be gone by now.
            if (!this.contentWindow) {
                console.info("Content window reference invalid, deleting document translation state");
                this.documentTranslationStateCommunicator.clear();
                return;
            }
            // Save results in extension state
            this.documentTranslationStateCommunicator.updatedDetectedLanguageResults(detectedLanguageResults);
            if (!detectedLanguageResults.confident) {
                console.debug("Language detection results not confident enough, bailing.");
                this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.LANGUAGE_NOT_DETECTED);
                return;
            }
            console.debug("Updating state to reflect that language has been detected");
            yield this.checkLanguageSupport(detectedLanguageResults);
        });
    }
    checkLanguageSupport(detectedLanguageResults) {
        return __awaiter(this, void 0, void 0, function* () {
            const { summarizeLanguageSupport } = new LanguageSupport_1.LanguageSupport();
            const detectedLanguage = detectedLanguageResults.language;
            const { acceptedTargetLanguages, 
            // defaultSourceLanguage,
            defaultTargetLanguage, supportedSourceLanguages, supportedTargetLanguagesGivenDefaultSourceLanguage, allPossiblySupportedTargetLanguages, } = yield summarizeLanguageSupport(detectedLanguageResults);
            if (acceptedTargetLanguages.includes(detectedLanguage)) {
                // Detected language is the same as the user's accepted target languages.
                console.info("Detected language is in one of the user's accepted target languages.", {
                    acceptedTargetLanguages,
                });
                this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.SOURCE_LANGUAGE_UNDERSTOOD);
                return;
            }
            if (!supportedSourceLanguages.includes(detectedLanguage)) {
                // Detected language is not part of the supported source languages.
                console.info("Detected language is not part of the supported source languages.", { detectedLanguage, supportedSourceLanguages });
                this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.TRANSLATION_UNSUPPORTED);
                return;
            }
            if (!allPossiblySupportedTargetLanguages.includes(defaultTargetLanguage)) {
                // Detected language is not part of the supported source languages.
                console.info("Default target language is not part of the supported target languages.", {
                    acceptedTargetLanguages,
                    defaultTargetLanguage,
                    allPossiblySupportedTargetLanguages,
                });
                this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.TRANSLATION_UNSUPPORTED);
                return;
            }
            if (!defaultTargetLanguage ||
                !supportedTargetLanguagesGivenDefaultSourceLanguage.includes(defaultTargetLanguage)) {
                // Combination of source and target languages unsupported
                console.info("Combination of source and target languages unsupported.", {
                    acceptedTargetLanguages,
                    defaultTargetLanguage,
                    supportedTargetLanguagesGivenDefaultSourceLanguage,
                });
                this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.TRANSLATION_UNSUPPORTED);
                return;
            }
            this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.OFFER);
        });
    }
    doTranslation(from, to) {
        return __awaiter(this, void 0, void 0, function* () {
            // If a TranslationDocument already exists for this document, it should
            // be used instead of creating a new one so that we can use the original
            // content of the page for the new translation instead of the newly
            // translated text.
            const translationDocument = this.contentWindow.translationDocument ||
                new TranslationDocument_1.TranslationDocument(this.document);
            console.info("Translating web page");
            this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.TRANSLATING);
            const domTranslator = new BergamotDomTranslator_1.BergamotDomTranslator(translationDocument, from, to);
            this.contentWindow.translationDocument = translationDocument;
            translationDocument.translatedFrom = from;
            translationDocument.translatedTo = to;
            translationDocument.translationError = false;
            try {
                console.info(`About to translate web page document (${translationDocument.translationRoots.length} translation items)`, { from, to });
                // TODO: Timeout here to be able to abort UI in case translation hangs
                yield domTranslator.translate((frameTranslationProgress) => {
                    this.documentTranslationStateCommunicator.broadcastUpdatedFrameTranslationProgress(frameTranslationProgress);
                });
                console.info(`Translation of web page document completed (translated ${translationDocument.translationRoots.filter(translationRoot => translationRoot.currentDisplayMode === "translation").length} out of ${translationDocument.translationRoots.length} translation items)`, { from, to });
                console.info("Translated web page");
                this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.TRANSLATED);
            }
            catch (err) {
                console.warn("Translation error occurred: ", err);
                translationDocument.translationError = true;
                this.documentTranslationStateCommunicator.broadcastUpdatedTranslationStatus(BaseTranslationState_1.TranslationStatus.ERROR);
            }
            finally {
                // Communicate that errors occurred
                // Positioned in finally-clause so that it gets communicated whether the
                // translation attempt resulted in some translated content or not
                domTranslator.errorsEncountered.forEach((error) => {
                    if (error.name === "BergamotTranslatorAPIModelLoadError") {
                        this.documentTranslationStateCommunicator.broadcastUpdatedAttributeValue("modelLoadErrorOccurred", true);
                    }
                    else if (error.name === "BergamotTranslatorAPIModelDownloadError") {
                        this.documentTranslationStateCommunicator.broadcastUpdatedAttributeValue("modelDownloadErrorOccurred", true);
                    }
                    else if (error.name === "BergamotTranslatorAPITranslationError") {
                        this.documentTranslationStateCommunicator.broadcastUpdatedAttributeValue("translationErrorOccurred", true);
                    }
                    else {
                        this.documentTranslationStateCommunicator.broadcastUpdatedAttributeValue("otherErrorOccurred", true);
                    }
                });
            }
        });
    }
    getDocumentTranslationStatistics() {
        return __awaiter(this, void 0, void 0, function* () {
            const translationDocument = this.contentWindow.translationDocument ||
                new TranslationDocument_1.TranslationDocument(this.document);
            const { translationRoots } = translationDocument;
            const { translationRootsVisible, translationRootsVisibleInViewport, } = yield translationDocument.determineVisibilityOfTranslationRoots();
            const generateOriginalMarkupToTranslate = translationRoot => translationDocument.generateMarkupToTranslate(translationRoot);
            const removeTags = originalString => {
                const detaggedString = detagAndProject_1.detag(originalString);
                return detaggedString.plainString;
            };
            const texts = translationRoots
                .map(generateOriginalMarkupToTranslate)
                .map(removeTags);
            const textsVisible = translationRootsVisible
                .map(generateOriginalMarkupToTranslate)
                .map(removeTags);
            const textsVisibleInViewport = translationRootsVisibleInViewport
                .map(generateOriginalMarkupToTranslate)
                .map(removeTags);
            const wordCount = texts.join(" ").split(" ").length;
            const wordCountVisible = textsVisible.join(" ").split(" ").length;
            const wordCountVisibleInViewport = textsVisibleInViewport
                .join(" ")
                .split(" ").length;
            const translationRootsCount = translationRoots.length;
            const simpleTranslationRootsCount = translationRoots.filter(translationRoot => translationRoot.isSimleTranslationRoot).length;
            return {
                translationRootsCount,
                simpleTranslationRootsCount,
                texts,
                textsVisible,
                textsVisibleInViewport,
                wordCount,
                wordCountVisible,
                wordCountVisibleInViewport,
            };
        });
    }
}
exports.DomTranslationManager = DomTranslationManager;


/***/ }),

/***/ 992:
/*!**********************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/TranslationDocument.ts ***!
  \**********************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.generateMarkupToTranslateForItem = exports.TranslationDocument = void 0;
const getTranslationNodes_1 = __webpack_require__(/*! ./getTranslationNodes */ 5173);
const TranslationItem_1 = __webpack_require__(/*! ./TranslationItem */ 6771);
/**
 * This class represents a document that is being translated,
 * and it is responsible for parsing the document,
 * generating the data structures translation (the list of
 * translation items and roots), and managing the original
 * and translated texts on the translation items.
 *
 * @param document  The document to be translated
 */
class TranslationDocument {
    constructor(document) {
        this.translatedFrom = "";
        this.translatedTo = "";
        this.translationError = false;
        this.originalShown = true;
        this.qualityEstimationShown = true;
        // Set temporarily to true during development to visually inspect which nodes have been processed and in which way
        this.paintProcessedNodes = false;
        this.nodeTranslationItemsMap = new Map();
        this.translationRoots = [];
        this._init(document);
    }
    /**
     * Initializes the object and populates
     * the translation roots lists.
     *
     * @param document  The document to be translated
     */
    _init(document) {
        // Get all the translation nodes in the document's body:
        // a translation node is a node from the document which
        // contains useful content for translation, and therefore
        // must be included in the translation process.
        const translationNodes = getTranslationNodes_1.getTranslationNodes(document.body);
        console.info(`The document has a total of ${translationNodes.length} translation nodes, of which ${translationNodes.filter(tn => tn.isTranslationRoot).length} are translation roots`);
        translationNodes.forEach((translationNode, index) => {
            const { content, isTranslationRoot } = translationNode;
            if (this.paintProcessedNodes) {
                content.style.backgroundColor = "darkorange";
            }
            // Create a TranslationItem object for this node.
            // This function will also add it to the this.translationRoots array.
            this._createItemForNode(content, index, isTranslationRoot);
        });
        // At first all translation roots are stored in the translation roots list, and only after
        // the process has finished we're able to determine which translation roots are
        // simple, and which ones are not.
        // A simple root is defined by a translation root with no children items, which
        // basically represents an element from a page with only text content
        // inside.
        // This distinction is useful for optimization purposes: we treat a
        // simple root as plain-text in the translation process and with that
        // we are able to reduce their data payload sent to the translation service.
        for (const translationRoot of this.translationRoots) {
            if (!translationRoot.children.length &&
                translationRoot.nodeRef instanceof Element &&
                translationRoot.nodeRef.childElementCount === 0) {
                translationRoot.isSimleTranslationRoot = true;
                if (this.paintProcessedNodes) {
                    translationRoot.nodeRef.style.backgroundColor = "orange";
                }
            }
        }
    }
    /**
     * Creates a TranslationItem object, which should be called
     * for each node returned by getTranslationNodes.
     *
     * @param node                The DOM node for this item.
     * @param id                  A unique, numeric id for this item.
     * @param isTranslationRoot   A boolean saying whether this item is a translation root.
     *
     * @returns           A TranslationItem object.
     */
    _createItemForNode(node, id, isTranslationRoot) {
        if (this.nodeTranslationItemsMap.has(node)) {
            return this.nodeTranslationItemsMap.get(node);
        }
        const item = new TranslationItem_1.TranslationItem(node, id, isTranslationRoot);
        if (isTranslationRoot) {
            // Translation root items do not have a parent item.
            this.translationRoots.push(item);
        }
        else {
            // Other translation nodes have at least one ancestor which is a translation root
            let ancestorTranslationItem;
            for (let ancestor = node.parentNode; ancestor; ancestor = ancestor.parentNode) {
                ancestorTranslationItem = this.nodeTranslationItemsMap.get(ancestor);
                if (ancestorTranslationItem) {
                    ancestorTranslationItem.children.push(item);
                    break;
                }
                else {
                    // make intermediate ancestors link to the descendent translation item
                    // so that it gets picked up on in generateOriginalStructureElements
                    this.nodeTranslationItemsMap.set(ancestor, item);
                }
            }
        }
        this.nodeTranslationItemsMap.set(node, item);
        return item;
    }
    /**
     * Generate the markup that represents a TranslationItem object.
     * Besides generating the markup, it also stores a fuller representation
     * of the TranslationItem in the "original" field of the TranslationItem object,
     * which needs to be stored for later to be used in the "Show Original" functionality.
     * If this function had already been called for the given item (determined
     * by the presence of the "original" array in the item), the markup will
     * be regenerated from the "original" data instead of from the related
     * DOM nodes (because the nodes might contain translated data).
     *
     * @param item     A TranslationItem object
     *
     * @returns        A markup representation of the TranslationItem.
     */
    generateMarkupToTranslate(item) {
        if (!item.original) {
            item.original = this.generateOriginalStructureElements(item);
        }
        return regenerateMarkupToTranslateFromOriginal(item);
    }
    /**
     * Generates a fuller representation of the TranslationItem
     * @param item
     */
    generateOriginalStructureElements(item) {
        const original = [];
        if (item.isSimleTranslationRoot) {
            const text = item.nodeRef.firstChild.nodeValue.trim();
            original.push(text);
            return original;
        }
        let wasLastItemPlaceholder = false;
        for (const child of Array.from(item.nodeRef.childNodes)) {
            if (child.nodeType === child.TEXT_NODE) {
                const x = child.nodeValue;
                const hasLeadingWhitespace = x.length !== x.trimStart().length;
                const hasTrailingWhitespace = x.length !== x.trimEnd().length;
                if (x.trim() !== "") {
                    const xWithNormalizedWhitespace = `${hasLeadingWhitespace ? " " : ""}${x.trim()}${hasTrailingWhitespace ? " " : ""}`;
                    original.push(xWithNormalizedWhitespace);
                    wasLastItemPlaceholder = false;
                }
                continue;
            }
            const objInMap = this.nodeTranslationItemsMap.get(child);
            if (objInMap && !objInMap.isTranslationRoot) {
                // If this childNode is present in the nodeTranslationItemsMap, it means
                // it's a translation node: it has useful content for translation.
                // In this case, we need to stringify this node.
                // However, if this item is a translation root, we should skip it here in this
                // object's child list (and just add a placeholder for it), because
                // it will be stringified separately for being a translation root.
                original.push(objInMap);
                objInMap.original = this.generateOriginalStructureElements(objInMap);
                wasLastItemPlaceholder = false;
            }
            else if (!wasLastItemPlaceholder) {
                // Otherwise, if this node doesn't contain any useful content,
                // or if it is a translation root itself, we can replace it with a placeholder node.
                // We can't simply eliminate this node from our string representation
                // because that could change the HTML structure (e.g., it would
                // probably merge two separate text nodes).
                // It's not necessary to add more than one placeholder in sequence;
                // we can optimize them away.
                original.push(new TranslationItem_1.TranslationItem_NodePlaceholder());
                wasLastItemPlaceholder = true;
            }
        }
        return original;
    }
    /**
     * Changes the document to display its translated
     * content.
     */
    showTranslation() {
        this.originalShown = false;
        this.qualityEstimationShown = false;
        this._swapDocumentContent("translation");
    }
    /**
     * Changes the document to display its original
     * content.
     */
    showOriginal() {
        this.originalShown = true;
        this.qualityEstimationShown = false;
        this._swapDocumentContent("original");
        // TranslationTelemetry.recordShowOriginalContent();
    }
    /**
     * Changes the document to display the translation with quality estimation metadata
     * content.
     */
    showQualityEstimation() {
        this.originalShown = false;
        this.qualityEstimationShown = true;
        this._swapDocumentContent("qeAnnotatedTranslation");
    }
    /**
     * Swap the document with the resulting translation,
     * or back with the original content.
     */
    _swapDocumentContent(target) {
        (() => __awaiter(this, void 0, void 0, function* () {
            this.translationRoots
                .filter(translationRoot => translationRoot.currentDisplayMode !== target)
                .forEach(translationRoot => translationRoot.swapText(target, this.paintProcessedNodes));
            // TODO: Make sure that the above does not lock the main event loop
            /*
            // Let the event loop breath on every 100 nodes
            // that are replaced.
            const YIELD_INTERVAL = 100;
            await Async.yieldingForEach(
              this.roots,
              root => root.swapText(target, this.paintProcessedNodes),
              YIELD_INTERVAL
            );
            */
        }))();
    }
    determineVisibilityOfTranslationRoots() {
        return __awaiter(this, void 0, void 0, function* () {
            const { translationRoots } = this;
            const elements = translationRoots.map(translationRoot => translationRoot.nodeRef);
            const elementsVisibleInViewport = yield getElementsVisibleInViewport(elements);
            const translationRootsVisible = [];
            const translationRootsVisibleInViewport = [];
            for (let i = 0; i < translationRoots.length; i++) {
                const translationRoot = translationRoots[i];
                const visible = isElementVisible(translationRoot.nodeRef);
                if (visible) {
                    translationRootsVisible.push(translationRoot);
                }
                const visibleInViewport = isElementVisibleInViewport(elementsVisibleInViewport, translationRoot.nodeRef);
                if (visibleInViewport) {
                    translationRootsVisibleInViewport.push(translationRoot);
                }
            }
            if (this.paintProcessedNodes) {
                translationRootsVisible.forEach(translationRoot => {
                    translationRoot.nodeRef.style.color = "purple";
                });
                translationRootsVisibleInViewport.forEach(translationRoot => {
                    translationRoot.nodeRef.style.color = "maroon";
                });
            }
            return {
                translationRootsVisible,
                translationRootsVisibleInViewport,
            };
        });
    }
}
exports.TranslationDocument = TranslationDocument;
/**
 * Generate the translation markup for a given item.
 *
 * @param   item       A TranslationItem object.
 * @param   content    The inner content for this item.
 * @returns string     The outer HTML needed for translation
 *                     of this item.
 */
function generateMarkupToTranslateForItem(item, content) {
    const localName = item.isTranslationRoot ? "div" : "b";
    return ("<" + localName + " id=n" + item.id + ">" + content + "</" + localName + ">");
}
exports.generateMarkupToTranslateForItem = generateMarkupToTranslateForItem;
/**
 * Regenerate the markup that represents a TranslationItem object,
 * with data from its "original" array. The array must have already
 * been created by TranslationDocument.generateMarkupToTranslate().
 *
 * @param item     A TranslationItem object
 *
 * @returns        A markup representation of the TranslationItem.
 */
function regenerateMarkupToTranslateFromOriginal(item) {
    if (item.isSimleTranslationRoot) {
        return item.original[0];
    }
    let str = "";
    for (const child of item.original) {
        if (child instanceof TranslationItem_1.TranslationItem) {
            str += regenerateMarkupToTranslateFromOriginal(child);
        }
        else if (child instanceof TranslationItem_1.TranslationItem_NodePlaceholder) {
            str += "<br>";
        }
        else {
            str += child;
        }
    }
    return generateMarkupToTranslateForItem(item, str);
}
const isElementVisible = (el) => {
    const rect = el.getBoundingClientRect();
    // Elements that are not visible will have a zero width/height bounding client rect
    return rect.width > 0 && rect.height > 0;
};
const isElementVisibleInViewport = (elementsVisibleInViewport, el) => {
    return !!elementsVisibleInViewport.filter($el => $el === el).length;
};
const getElementsVisibleInViewport = (elements) => __awaiter(void 0, void 0, void 0, function* () {
    return new Promise(resolve => {
        const options = {
            threshold: 0.0,
        };
        const callback = (entries, $observer) => {
            // console.debug("InteractionObserver callback", entries.length, entries);
            const elementsInViewport = entries
                .filter(entry => entry.isIntersecting)
                .map(entry => entry.target);
            $observer.disconnect();
            resolve(elementsInViewport);
        };
        const observer = new IntersectionObserver(callback, options);
        elements.forEach(el => observer.observe(el));
    });
});


/***/ }),

/***/ 6771:
/*!******************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/TranslationItem.ts ***!
  \******************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.TranslationItem_NodePlaceholder = exports.TranslationItem = void 0;
/**
 * This class represents an item for translation. It's basically our
 * wrapper class around a node returned by getTranslationNode, with
 * more data and structural information on it.
 *
 * At the end of the translation process, besides the properties below,
 * a TranslationItem will contain two other properties: one called "original"
 * and one called "translation". They are twin objects, one which reflect
 * the structure of that node in its original state, and the other in its
 * translated state.
 *
 * The "original" array is generated in the
 * TranslationDocument.generateMarkupToTranslate function,
 * and the "translation" array is generated when the translation results
 * are parsed.
 *
 * They are both arrays, which contain a mix of strings and references to
 * child TranslationItems. The references in both arrays point to the * same *
 * TranslationItem object, but they might appear in different orders between the
 * "original" and "translation" arrays.
 *
 * An example:
 *
 *  English: <div id="n1">Welcome to <b id="n2">Mozilla's</b> website</div>
 *  Portuguese: <div id="n1">Bem vindo a pagina <b id="n2">da Mozilla</b></div>
 *
 *  TranslationItem n1 = {
 *    id: 1,
 *    original: ["Welcome to", ptr to n2, "website"]
 *    translation: ["Bem vindo a pagina", ptr to n2]
 *  }
 *
 *  TranslationItem n2 = {
 *    id: 2,
 *    original: ["Mozilla's"],
 *    translation: ["da Mozilla"]
 *  }
 */
class TranslationItem {
    constructor(node, id, isTranslationRoot) {
        this.isTranslationRoot = false;
        this.isSimleTranslationRoot = false;
        this.children = [];
        this.nodeRef = node;
        this.id = id;
        this.isTranslationRoot = isTranslationRoot;
    }
    toString() {
        let rootType = "";
        if (this.isTranslationRoot) {
            if (this.isSimleTranslationRoot) {
                rootType = " (simple root)";
            }
            else {
                rootType = " (non simple root)";
            }
        }
        return ("[object TranslationItem: <" +
            this.nodeRef.toString() +
            ">" +
            rootType +
            "]");
    }
    /**
     * This function will parse the result of the translation of one translation
     * item. If this item was a simple root, all we sent was a plain-text version
     * of it, so the result is also straightforward text.
     *
     * For non-simple roots, we sent a simplified HTML representation of that
     * node, and we'll first parse that into an HTML doc and then call the
     * parseResultNode helper function to parse it.
     *
     * While parsing, the result is stored in the "translation" field of the
     * TranslationItem, which will be used to display the final translation when
     * all items are finished. It remains stored too to allow back-and-forth
     * switching between the "Show Original" and "Show Translation" functions.
     *
     * @param translatedMarkup    A string with the textual result received from the translation engine,
     *                            which can be plain-text or a serialized HTML doc.
     */
    parseTranslationResult(translatedMarkup) {
        this.translatedMarkup = translatedMarkup;
        if (this.isSimleTranslationRoot) {
            // If translation contains HTML entities, we need to convert them.
            // It is because simple roots expect a plain text result.
            if (this.isSimleTranslationRoot &&
                translatedMarkup.match(/&([a-z0-9]+|#[0-9]{1,6}|#x[0-9a-f]{1,6});/gi)) {
                const doc = new DOMParser().parseFromString(translatedMarkup, "text/html");
                translatedMarkup = doc.body.firstChild.nodeValue;
            }
            this.translation = [translatedMarkup];
            return;
        }
        const domParser = new DOMParser();
        const doc = domParser.parseFromString(translatedMarkup, "text/html");
        this.translation = [];
        parseResultNode(this, doc.body.firstChild, "translation");
    }
    /**
     * Note: QE-annotated translation results are never plain text nodes, despite that
     * the original translation item may be a simple translation root.
     * This wreaks havoc.
     * @param qeAnnotatedTranslatedMarkup
     */
    parseQeAnnotatedTranslationResult(qeAnnotatedTranslatedMarkup) {
        const domParser = new DOMParser();
        const doc = domParser.parseFromString(qeAnnotatedTranslatedMarkup, "text/html");
        this.qeAnnotatedTranslation = [];
        parseResultNode(this, doc.body.firstChild, "qeAnnotatedTranslation");
    }
    /**
     * This function finds a child TranslationItem
     * with the given id.
     * @param id        The id to look for, in the format "n#"
     * @returns         A TranslationItem with the given id, or null if
     *                  it was not found.
     */
    getChildById(id) {
        for (const child of this.children) {
            const childId = "n" + child.id;
            if (childId === id) {
                return child;
            }
        }
        return null;
    }
    /**
     * Swap the text of this TranslationItem between
     * its original and translated states.
     */
    swapText(target, paintProcessedNodes) {
        swapTextForItem(this, target, paintProcessedNodes);
    }
}
exports.TranslationItem = TranslationItem;
/**
 * This object represents a placeholder item for translation. It's similar to
 * the TranslationItem class, but it represents nodes that have no meaningful
 * content for translation. These nodes will be replaced by "<br>" in a
 * translation request. It's necessary to keep them to use it as a mark
 * for correct positioning and splitting of text nodes.
 */
class TranslationItem_NodePlaceholder {
    static toString() {
        return "[object TranslationItem_NodePlaceholder]";
    }
}
exports.TranslationItem_NodePlaceholder = TranslationItem_NodePlaceholder;
/**
 * Helper function to parse a HTML doc result.
 * How it works:
 *
 * An example result string is:
 *
 * <div id="n1">Hello <b id="n2">World</b> of Mozilla.</div>
 *
 * For an element node, we look at its id and find the corresponding
 * TranslationItem that was associated with this node, and then we
 * walk down it repeating the process.
 *
 * For text nodes we simply add it as a string.
 */
function parseResultNode(item, node, target) {
    try {
        const into = item[target];
        // @ts-ignore
        for (const child of node.childNodes) {
            if (child.nodeType === Node.TEXT_NODE) {
                into.push(child.nodeValue);
            }
            else if (child.localName === "br") {
                into.push(new TranslationItem_NodePlaceholder());
            }
            else if (child.dataset &&
                typeof child.dataset.translationQeScore !== "undefined") {
                // handle the special case of quality estimate annotated nodes
                into.push(child);
            }
            else {
                const translationRootChild = item.getChildById(child.id);
                if (translationRootChild) {
                    into.push(translationRootChild);
                    translationRootChild[target] = [];
                    parseResultNode(translationRootChild, child, target);
                }
                else {
                    console.warn(`Result node's (belonging to translation item with id ${item.id}) child node (child.id: ${child.id}) lacks an associated translation root child`, { item, node, child });
                }
            }
        }
    }
    catch (err) {
        console.error(err);
        throw err;
    }
}
/* eslint-disable complexity */
// TODO: Simplify swapTextForItem to avoid the following eslint error:
// Function 'swapTextForItem' has a complexity of 35. Maximum allowed is 34  complexity
/**
 * Helper function to swap the text of a TranslationItem
 * between its original and translated states.
 * How it works:
 *
 * The function iterates through the target array (either the `original` or
 * `translation` array from the TranslationItem), while also keeping a pointer
 * to a current position in the child nodes from the actual DOM node that we
 * are modifying. This pointer is moved forward after each item of the array
 * is translated. If, at any given time, the pointer doesn't match the expected
 * node that was supposed to be seen, it means that the original and translated
 * contents have a different ordering, and thus we need to adjust that.
 *
 * A full example of the reordering process, swapping from Original to
 * Translation:
 *
 *    Original (en): <div>I <em>miss</em> <b>you</b></div>
 *
 * Translation (fr): <div><b>Tu</b> me <em>manques</em></div>
 *
 * Step 1:
 *   pointer points to firstChild of the DOM node, textnode "I "
 *   first item in item.translation is [object TranslationItem <b>]
 *
 *   pointer does not match the expected element, <b>. So let's move <b> to the
 *   pointer position.
 *
 *   Current state of the DOM:
 *     <div><b>you</b>I <em>miss</em> </div>
 *
 * Step 2:
 *   pointer moves forward to nextSibling, textnode "I " again.
 *   second item in item.translation is the string " me "
 *
 *   pointer points to a text node, and we were expecting a text node. Match!
 *   just replace the text content.
 *
 *   Current state of the DOM:
 *     <div><b>you</b> me <em>miss</em> </div>
 *
 * Step 3:
 *   pointer moves forward to nextSibling, <em>miss</em>
 *   third item in item.translation is [object TranslationItem <em>]
 *
 *   pointer points to the expected node. Match! Nothing to do.
 *
 * Step 4:
 *   all items in this item.translation were transformed. The remaining
 *   text nodes are cleared to "", and domNode.normalize() removes them.
 *
 *   Current state of the DOM:
 *     <div><b>you</b> me <em>miss</em></div>
 *
 * Further steps:
 *   After that, the function will visit the child items (from the visitStack),
 *   and the text inside the <b> and <em> nodes will be swapped as well,
 *   yielding the final result:
 *
 *     <div><b>Tu</b> me <em>manques</em></div>
 */
function swapTextForItem(item, target, paintProcessedNodes) {
    // visitStack is the stack of items that we still need to visit.
    // Let's start the process by adding the translation root item.
    const visitStack = [item];
    if (paintProcessedNodes) {
        item.nodeRef.style.border = "1px solid maroon";
    }
    while (visitStack.length) {
        const curItem = visitStack.shift();
        if (paintProcessedNodes) {
            item.nodeRef.style.border = "1px solid yellow";
        }
        const domNode = curItem.nodeRef;
        if (!domNode) {
            // Skipping this item due to a missing node.
            continue;
        }
        if (!curItem[target]) {
            // Translation not found for this item. This could be due to
            // the translation not yet being available from the translation engine
            // For example, if a translation was broken in various
            // chunks, and not all of them has completed, the items from that
            // chunk will be missing its "translation" field.
            if (paintProcessedNodes) {
                curItem.nodeRef.style.border = "1px solid red";
            }
            continue;
        }
        domNode.normalize();
        if (paintProcessedNodes) {
            curItem.nodeRef.style.border = "1px solid green";
        }
        // curNode points to the child nodes of the DOM node that we are
        // modifying. During most of the process, while the target array is
        // being iterated (in the for loop below), it should walk together with
        // the array and be pointing to the correct node that needs to modified.
        // If it's not pointing to it, that means some sort of node reordering
        // will be necessary to produce the correct translation.
        // Note that text nodes don't need to be reordered, as we can just replace
        // the content of one text node with another.
        //
        // curNode starts in the firstChild...
        let curNode = domNode.firstChild;
        if (paintProcessedNodes && curNode instanceof HTMLElement) {
            curNode.style.border = "1px solid blue";
        }
        // ... actually, let's make curNode start at the first useful node (either
        // a non-blank text node or something else). This is not strictly necessary,
        // as the reordering algorithm would correctly handle this case. However,
        // this better aligns the resulting translation with the DOM content of the
        // page, avoiding cases that would need to be unnecessarily reordered.
        //
        // An example of how this helps:
        //
        // ---- Original: <div>                <b>Hello </b> world.</div>
        //                       ^textnode 1      ^item 1      ^textnode 2
        //
        // - Translation: <div><b>Hallo </b> Welt.</div>
        //
        // Transformation process without this optimization:
        //   1 - start pointer at textnode 1
        //   2 - move item 1 to first position inside the <div>
        //
        //       Node now looks like: <div><b>Hello </b>[         ][ world.]</div>
        //                                         textnode 1^       ^textnode 2
        //
        //   3 - replace textnode 1 with " Welt."
        //   4 - clear remaining text nodes (in this case, textnode 2)
        //
        // Transformation process with this optimization:
        //   1 - start pointer at item 1
        //   2 - item 1 is already in position
        //   3 - replace textnode 2 with " Welt."
        //
        // which completely avoids any node reordering, and requires only one
        // text change instead of two (while also leaving the page closer to
        // its original state).
        while (curNode &&
            curNode.nodeType === Node.TEXT_NODE &&
            curNode.nodeValue.trim() === "") {
            curNode = curNode.nextSibling;
        }
        // Now let's walk through all items in the `target` array of the
        // TranslationItem. This means either the TranslationItem.original or
        // TranslationItem.translation array.
        for (const targetItem of curItem[target]) {
            if (targetItem instanceof TranslationItem) {
                // If the array element is another TranslationItem object, let's
                // add it to the stack to be visited.
                visitStack.push(targetItem);
                const targetNode = targetItem.nodeRef;
                // If the node is not in the expected position, let's reorder
                // it into position...
                if (curNode !== targetNode &&
                    // ...unless the page has reparented this node under a totally
                    // different node (or removed it). In this case, all bets are off
                    // on being able to do anything correctly, so it's better not to
                    // bring back the node to this parent.
                    // @ts-ignore
                    targetNode.parentNode === domNode) {
                    // We don't need to null-check curNode because insertBefore(..., null)
                    // does what we need in that case: reorder this node to the end
                    // of child nodes.
                    domNode.insertBefore(targetNode, curNode);
                    curNode = targetNode;
                }
                // Move pointer forward. Since we do not add empty text nodes to the
                // list of translation items, we must skip them here too while
                // traversing the DOM in order to get better alignment between the
                // text nodes and the translation items.
                if (curNode) {
                    curNode = getNextSiblingSkippingEmptyTextNodes(curNode);
                }
            }
            else if (targetItem instanceof TranslationItem_NodePlaceholder) {
                // If the current item is a placeholder node, we need to move
                // our pointer "past" it, jumping from one side of a block of
                // elements + empty text nodes to the other side. Even if
                // non-placeholder elements exists inside the jumped block,
                // they will be pulled correctly later in the process when the
                // targetItem for those nodes are handled.
                while (curNode &&
                    (curNode.nodeType !== Node.TEXT_NODE ||
                        curNode.nodeValue.trim() === "")) {
                    curNode = curNode.nextSibling;
                }
            }
            else {
                // Finally, if it's a text item, we just need to find the next
                // text node to use. Text nodes don't need to be reordered, so
                // the first one found can be used.
                while (curNode && curNode.nodeType !== Node.TEXT_NODE) {
                    curNode = curNode.nextSibling;
                }
                // If none was found and we reached the end of the child nodes,
                // let's create a new one.
                if (!curNode) {
                    // We don't know if the original content had a space or not,
                    // so the best bet is to create the text node with " " which
                    // will add one space at the beginning and one at the end.
                    curNode = domNode.appendChild(domNode.ownerDocument.createTextNode(" "));
                }
                if (target === "translation") {
                    // A trailing and a leading space must be preserved because
                    // they are meaningful in HTML.
                    const preSpace = /^\s/.test(curNode.nodeValue) ? " " : "";
                    const endSpace = /\s$/.test(curNode.nodeValue) ? " " : "";
                    curNode.nodeValue = preSpace + targetItem + endSpace;
                }
                else {
                    curNode.nodeValue = targetItem;
                }
                if (["original", "translation"].includes(target)) {
                    // Workaround necessary when switching "back" from QE display
                    // since quality estimated annotated nodes
                    // replaced the simple text nodes of the original document
                    // @ts-ignore
                    for (const child of curNode.parentNode.childNodes) {
                        if (child.dataset &&
                            typeof child.dataset.translationQeScore !== "undefined") {
                            // There should be only 1 such node. Remove the curNode from the
                            // parent's children and replace the qe-annotated node with it to
                            // maintain the right order in original DOM tree of the document.
                            curNode.remove();
                            child.parentNode.replaceChild(curNode, child);
                        }
                    }
                    curNode = getNextSiblingSkippingEmptyTextNodes(curNode);
                }
                else if (target === "qeAnnotatedTranslation") {
                    const nextSibling = getNextSiblingSkippingEmptyTextNodes(curNode);
                    // Replace the text node with the qe-annotated node to maintain the
                    // right order in original DOM tree of the document.
                    curNode.parentNode.replaceChild(targetItem, curNode);
                    curNode = nextSibling;
                }
            }
        }
        // The translated version of a node might have less text nodes than its
        // original version. If that's the case, let's clear the remaining nodes.
        if (curNode) {
            clearRemainingNonEmptyTextNodesFromElement(curNode);
        }
        // And remove any garbage "" nodes left after clearing.
        domNode.normalize();
        // Mark the translation item as displayed in the requested target
        curItem.currentDisplayMode = target;
        if (paintProcessedNodes) {
            curItem.nodeRef.style.border = "2px solid green";
        }
    }
}
/* eslint-enable complexity */
function getNextSiblingSkippingEmptyTextNodes(startSibling) {
    let item = startSibling.nextSibling;
    while (item &&
        item.nodeType === Node.TEXT_NODE &&
        item.nodeValue.trim() === "") {
        item = item.nextSibling;
    }
    return item;
}
function clearRemainingNonEmptyTextNodesFromElement(startSibling) {
    let item = startSibling;
    while (item) {
        if (item.nodeType === Node.TEXT_NODE && item.nodeValue !== "") {
            item.nodeValue = "";
        }
        item = item.nextSibling;
    }
}


/***/ }),

/***/ 1435:
/*!************************************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/dom-translators/BaseDomTranslator.ts ***!
  \************************************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.BaseDomTranslator = exports.DomTranslatorError = void 0;
const MinimalDomTranslator_1 = __webpack_require__(/*! ./MinimalDomTranslator */ 1975);
class DomTranslatorError extends Error {
    constructor() {
        super(...arguments);
        this.name = "DomTranslatorError";
    }
}
exports.DomTranslatorError = DomTranslatorError;
/**
 * Base class for DOM translators that splits the document into several chunks
 * respecting the data limits of the backing API.
 */
class BaseDomTranslator extends MinimalDomTranslator_1.MinimalDomTranslator {
    /**
     * @param translationDocument  The TranslationDocument object that represents
     *                             the webpage to be translated
     * @param sourceLanguage       The source language of the document
     * @param targetLanguage       The target language for the translation
     * @param translationApiClient
     * @param parseChunkResult
     * @param translationApiLimits
     * @param domTranslatorRequestFactory
     */
    constructor(translationDocument, sourceLanguage, targetLanguage, translationApiClient, parseChunkResult, translationApiLimits, domTranslatorRequestFactory) {
        super(translationDocument, sourceLanguage, targetLanguage);
        this.translatedCharacterCount = 0;
        this.errorsEncountered = [];
        this.partialSuccess = false;
        this.translationApiClient = translationApiClient;
        this.parseChunkResult = parseChunkResult;
        this.translationApiLimits = translationApiLimits;
        this.domTranslatorRequestFactory = domTranslatorRequestFactory;
    }
    /**
     * Performs the translation, splitting the document into several chunks
     * respecting the data limits of the API.
     *
     * @returns {Promise}          A promise that will resolve when the translation
     *                             task is finished.
     */
    translate(frameTranslationProgressCallback) {
        return __awaiter(this, void 0, void 0, function* () {
            const chunksBeingProcessed = [];
            const { MAX_REQUESTS } = this.translationApiLimits;
            const { translationRoots } = this.translationDocument;
            const { translationRootsVisible, translationRootsVisibleInViewport, } = yield this.translationDocument.determineVisibilityOfTranslationRoots();
            this.translationRootsPickedUpForTranslation = [];
            const progressOfIndividualTranslationRequests = new Map();
            // Split the document into various requests to be sent to the translation API
            for (let currentRequestOrdinal = 0; currentRequestOrdinal < MAX_REQUESTS; currentRequestOrdinal++) {
                // Determine the data for the next request.
                const domTranslationChunk = this.generateNextDomTranslationChunk(translationRoots, translationRootsVisible, translationRootsVisibleInViewport);
                // Break if there was nothing left to translate
                if (domTranslationChunk.translationRoots.length === 0) {
                    break;
                }
                // Create a real request for the translation engine and add it to the pending requests list.
                const translationRequestData = domTranslationChunk.translationRequestData;
                const domTranslatorRequest = this.domTranslatorRequestFactory(translationRequestData, this.sourceLanguage, this.targetLanguage);
                // Fire off the requests in parallel to existing requests
                const chunkBeingProcessed = domTranslatorRequest.fireRequest(this.translationApiClient, (translationRequestProgress) => {
                    progressOfIndividualTranslationRequests.set(translationRequestProgress.requestId, translationRequestProgress);
                    frameTranslationProgressCallback({
                        progressOfIndividualTranslationRequests,
                    });
                });
                chunksBeingProcessed.push(chunkBeingProcessed);
                chunkBeingProcessed
                    .then((translationResponseData) => {
                    if (translationResponseData) {
                        this.chunkCompleted(translationResponseData, domTranslationChunk, domTranslatorRequest);
                    }
                    else {
                        throw new Error("The returned translationResponseData was false/empty");
                    }
                })
                    .catch(err => {
                    this.errorsEncountered.push(err);
                });
                console.info(`Fired off request with ${domTranslationChunk.translationRoots.length} translation roots to the translation backend`, { domTranslationChunk });
                if (domTranslationChunk.isLastChunk) {
                    break;
                }
            }
            // Return early with a noop if there is nothing to translate
            if (chunksBeingProcessed.length === 0) {
                console.info("Found nothing to translate");
                return { characterCount: 0 };
            }
            console.info(`Fired off ${chunksBeingProcessed.length} requests to the translation backend`);
            // Wait for all requests to settle
            yield Promise.allSettled(chunksBeingProcessed);
            // Surface encountered errors
            if (this.errorsEncountered.length) {
                console.warn("Errors were encountered during translation", this.errorsEncountered);
            }
            // If at least one chunk was successful, the
            // translation should be displayed, albeit incomplete.
            // Otherwise, the "Error" state will appear.
            if (!this.partialSuccess) {
                throw new DomTranslatorError("No content was translated");
            }
            return {
                characterCount: this.translatedCharacterCount,
            };
        });
    }
    /**
     * Function called when a request sent to the translation engine completed successfully.
     * This function handles calling the function to parse the result and the
     * function to resolve the promise returned by the public `translate()`
     * method when there's no pending request left.
     */
    chunkCompleted(translationResponseData, domTranslationChunk, domTranslatorRequest) {
        if (this.parseChunkResult(translationResponseData, domTranslationChunk)) {
            this.partialSuccess = true;
            // Count the number of characters successfully translated.
            this.translatedCharacterCount += domTranslatorRequest.characterCount;
            // Show translated chunks as they arrive
            console.info("Part of the web page document translated. Showing translations that have completed so far...");
            this.translationDocument.showTranslation();
        }
    }
    /**
     * This function will determine what is the data to be used for
     * the Nth request we are generating, based on the input params.
     */
    generateNextDomTranslationChunk(translationRoots, translationRootsVisible, translationRootsVisibleInViewport) {
        let currentDataSize = 0;
        let currentChunks = 0;
        const translationRequestData = {
            markupsToTranslate: [],
        };
        const chunkTranslationRoots = [];
        const { MAX_REQUEST_DATA, MAX_REQUEST_TEXTS } = this.translationApiLimits;
        let translationRootsToConsider;
        // Don't consider translation roots that are already picked up for translation
        const notYetPickedUp = ($translationRoots) => $translationRoots.filter(value => !this.translationRootsPickedUpForTranslation.includes(value));
        // Prioritize the translation roots visible in viewport
        translationRootsToConsider = notYetPickedUp(translationRootsVisibleInViewport);
        // Then prioritize the translation roots that are visible
        if (translationRootsToConsider.length === 0) {
            translationRootsToConsider = notYetPickedUp(translationRootsVisible);
        }
        // Then prioritize the remaining translation roots
        if (translationRootsToConsider.length === 0) {
            translationRootsToConsider = notYetPickedUp(translationRoots);
        }
        for (let i = 0; i < translationRootsToConsider.length; i++) {
            const translationRoot = translationRootsToConsider[i];
            const markupToTranslate = this.translationDocument.generateMarkupToTranslate(translationRoot);
            const newCurSize = currentDataSize + markupToTranslate.length;
            const newChunks = currentChunks + 1;
            if (newCurSize > MAX_REQUEST_DATA || newChunks > MAX_REQUEST_TEXTS) {
                // If we've reached the API limits, let's stop accumulating data
                // for this request and return. We return information useful for
                // the caller to pass back on the next call, so that the function
                // can keep working from where it stopped.
                console.info("We have reached the specified translation API limits and will process remaining translation roots in a separate request", {
                    newCurSize,
                    newChunks,
                    translationApiLimits: this.translationApiLimits,
                });
                return {
                    translationRequestData,
                    translationRoots: chunkTranslationRoots,
                    isLastChunk: false,
                };
            }
            currentDataSize = newCurSize;
            currentChunks = newChunks;
            chunkTranslationRoots.push(translationRoot);
            this.translationRootsPickedUpForTranslation.push(translationRoot);
            translationRequestData.markupsToTranslate.push(markupToTranslate);
        }
        const remainingTranslationRoots = notYetPickedUp(translationRoots);
        const isLastChunk = remainingTranslationRoots.length === 0;
        return {
            translationRequestData,
            translationRoots: chunkTranslationRoots,
            isLastChunk,
        };
    }
}
exports.BaseDomTranslator = BaseDomTranslator;


/***/ }),

/***/ 1518:
/*!****************************************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/dom-translators/BergamotDomTranslator.ts ***!
  \****************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.BergamotDomTranslator = exports.MAX_REQUESTS = exports.MAX_REQUEST_TEXTS = exports.MAX_REQUEST_DATA = void 0;
const ContentScriptBergamotApiClient_1 = __webpack_require__(/*! ../../../shared-resources/ContentScriptBergamotApiClient */ 449);
const TranslationDocument_1 = __webpack_require__(/*! ../TranslationDocument */ 992);
const BaseDomTranslator_1 = __webpack_require__(/*! ./BaseDomTranslator */ 1435);
const BergamotDomTranslatorRequest_1 = __webpack_require__(/*! ./BergamotDomTranslatorRequest */ 9943);
// The maximum amount of net data allowed per request on Bergamot's API.
exports.MAX_REQUEST_DATA = 500000;
// The maximum number of texts allowed to be translated in a single request.
exports.MAX_REQUEST_TEXTS = 100;
// Self-imposed limit of requests. This means that a page that would need
// to be broken in more than this amount of requests won't be fully translated.
exports.MAX_REQUESTS = 15;
/**
 * Translates a webpage using Bergamot's Translation backend.
 */
class BergamotDomTranslator extends BaseDomTranslator_1.BaseDomTranslator {
    /**
     * @param translationDocument  The TranslationDocument object that represents
     *                             the webpage to be translated
     * @param sourceLanguage       The source language of the document
     * @param targetLanguage       The target language for the translation
     */
    constructor(translationDocument, sourceLanguage, targetLanguage) {
        super(translationDocument, sourceLanguage, targetLanguage, new ContentScriptBergamotApiClient_1.ContentScriptBergamotApiClient(), parseChunkResult, { MAX_REQUEST_DATA: exports.MAX_REQUEST_DATA, MAX_REQUEST_TEXTS: exports.MAX_REQUEST_TEXTS, MAX_REQUESTS: exports.MAX_REQUESTS }, (translationRequestData, $sourceLanguage, $targetLanguage) => new BergamotDomTranslatorRequest_1.BergamotDomTranslatorRequest(translationRequestData, $sourceLanguage, $targetLanguage));
    }
}
exports.BergamotDomTranslator = BergamotDomTranslator;
/**
 * This function parses the result returned by Bergamot's Translation API for
 * the translated text in the target language.
 *
 * @returns boolean      True if parsing of this chunk was successful.
 */
function parseChunkResult(translationResponseData, domTranslationChunk) {
    const len = translationResponseData.translatedMarkups.length;
    if (len === 0) {
        throw new Error("Translation response data has no translated strings");
    }
    if (len !== domTranslationChunk.translationRoots.length) {
        // This should never happen, but if the service returns a different number
        // of items (from the number of items submitted), we can't use this chunk
        // because all items would be paired incorrectly.
        throw new Error("Translation response data has a different number of items (from the number of items submitted)");
    }
    console.info(`Parsing translation chunk result with ${len} translation entries`);
    let errorOccurred = false;
    domTranslationChunk.translationRoots.forEach((translationRoot, index) => {
        try {
            const translatedMarkup = translationResponseData.translatedMarkups[index];
            translationRoot.parseTranslationResult(translatedMarkup);
            if (translationResponseData.qeAnnotatedTranslatedMarkups) {
                let qeAnnotatedTranslatedMarkup = translationResponseData.qeAnnotatedTranslatedMarkups[index];
                qeAnnotatedTranslatedMarkup = TranslationDocument_1.generateMarkupToTranslateForItem(translationRoot, qeAnnotatedTranslatedMarkup);
                translationRoot.parseQeAnnotatedTranslationResult(qeAnnotatedTranslatedMarkup);
            }
        }
        catch (e) {
            errorOccurred = true;
            console.error("Translation error: ", e);
        }
    });
    console.info(`Parsed translation chunk result with ${len} translation entries`, { errorOccurred });
    return !errorOccurred;
}


/***/ }),

/***/ 9943:
/*!***********************************************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/dom-translators/BergamotDomTranslatorRequest.ts ***!
  \***********************************************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.BergamotDomTranslatorRequest = void 0;
const detagAndProject_1 = __webpack_require__(/*! ./detagAndProject */ 961);
/**
 * Represents a request (for 1 chunk) sent off to Bergamot's translation backend.
 *
 * @params translationRequestData  The data to be used for this translation.
 * @param sourceLanguage           The source language of the document.
 * @param targetLanguage           The target language for the translation.
 * @param characterCount           A counter for tracking the amount of characters translated.
 *
 */
class BergamotDomTranslatorRequest {
    constructor(translationRequestData, sourceLanguage, targetLanguage) {
        this.translationRequestData = translationRequestData;
        this.sourceLanguage = sourceLanguage;
        this.targetLanguage = targetLanguage;
        this.translationRequestData.markupsToTranslate.forEach(text => {
            this.characterCount += text.length;
        });
    }
    /**
     * Initiates the request
     */
    fireRequest(bergamotApiClient, translationRequestProgressCallback) {
        return __awaiter(this, void 0, void 0, function* () {
            // The server can only deal with pure text, so we detag the strings to
            // translate and later project the tags back into the result
            const detaggedStrings = this.translationRequestData.markupsToTranslate.map(detagAndProject_1.detag);
            const plainStringsToTranslate = detaggedStrings.map(detaggedString => detaggedString.plainString);
            const results = yield bergamotApiClient.sendTranslationRequest(plainStringsToTranslate, this.sourceLanguage, this.targetLanguage, translationRequestProgressCallback);
            return Object.assign(Object.assign({}, this.parseResults(results, detaggedStrings)), { plainStringsToTranslate });
        });
    }
    parseResults(results, detaggedStrings) {
        const len = results.translatedTexts.length;
        if (len !== this.translationRequestData.markupsToTranslate.length) {
            // This should never happen, but if the service returns a different number
            // of items (from the number of items submitted), we can't use this chunk
            // because all items would be paired incorrectly.
            throw new Error("Translation backend returned a different number of results (from the number of strings to translate)");
        }
        const translatedMarkups = [];
        const translatedPlainTextStrings = [];
        const qeAnnotatedTranslatedMarkups = results.qeAnnotatedTranslatedTexts;
        // The 'text' field of results is a list of 'Paragraph'. Parse each 'Paragraph' entry
        results.translatedTexts.forEach((translatedPlainTextString, index) => {
            const detaggedString = detaggedStrings[index];
            // Work around issue with doubled periods returned at the end of the translated string
            const originalEndedWithASinglePeriod = /([^\.])\.(\s+)?$/gm.exec(detaggedString.plainString);
            const translationEndsWithTwoPeriods = /([^\.])\.\.(\s+)?$/gm.exec(translatedPlainTextString);
            if (originalEndedWithASinglePeriod && translationEndsWithTwoPeriods) {
                translatedPlainTextString = translatedPlainTextString.replace(/([^\.])\.\.(\s+)?$/gm, "$1.$2");
            }
            let translatedMarkup;
            // Use original rather than an empty or obviously invalid translation
            // TODO: Address this upstream
            if (["", "*", "* ()"].includes(translatedPlainTextString)) {
                translatedMarkup = this.translationRequestData.markupsToTranslate[index];
            }
            else {
                // Project original tags/markup onto translated plain text string
                // TODO: Use alignment info returned from the translation engine when it becomes available
                translatedMarkup = detagAndProject_1.project(detaggedString, translatedPlainTextString);
            }
            translatedMarkups.push(translatedMarkup);
            translatedPlainTextStrings.push(translatedPlainTextString);
        });
        return {
            translatedMarkups,
            translatedPlainTextStrings,
            qeAnnotatedTranslatedMarkups,
        };
    }
}
exports.BergamotDomTranslatorRequest = BergamotDomTranslatorRequest;


/***/ }),

/***/ 1975:
/*!***************************************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/dom-translators/MinimalDomTranslator.ts ***!
  \***************************************************************************************************************/
/***/ (function(__unused_webpack_module, exports) {


var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.MinimalDomTranslator = void 0;
class MinimalDomTranslator {
    /**
     * @param translationDocument  The TranslationDocument object that represents
     *                             the webpage to be translated
     * @param sourceLanguage       The source language of the document
     * @param targetLanguage       The target language for the translation
     */
    constructor(translationDocument, sourceLanguage, targetLanguage) {
        this.translationDocument = translationDocument;
        this.sourceLanguage = sourceLanguage;
        this.targetLanguage = targetLanguage;
    }
    translate(_translationProgressCallback) {
        return __awaiter(this, void 0, void 0, function* () {
            return { characterCount: -1 };
        });
    }
}
exports.MinimalDomTranslator = MinimalDomTranslator;


/***/ }),

/***/ 961:
/*!**********************************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/dom-translators/detagAndProject.ts ***!
  \**********************************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {


Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.project = exports.detag = void 0;
const isTagTokenWithImpliedWhitespace = (token) => {
    return token.type === "tag" && token.tagName === "br";
};
const detag = (originalString) => {
    // console.info("detag", { originalString });
    const originalStringDoc = new DOMParser().parseFromString(originalString, "text/html");
    // console.debug("originalStringDoc.body", originalStringDoc.body);
    const tokens = serializeNodeIntoTokens(originalStringDoc.body);
    // console.debug({ tokens });
    const plainString = tokens
        .map(token => {
        if (token.type === "tag") {
            return isTagTokenWithImpliedWhitespace(token) ? " " : "";
        }
        return token.textRepresentation;
    })
        .join("");
    return {
        originalString,
        tokens,
        plainString,
    };
};
exports.detag = detag;
function serializeNodeIntoTokens(node) {
    const tokens = [];
    try {
        // @ts-ignore
        for (const child of node.childNodes) {
            if (child.nodeType === Node.TEXT_NODE) {
                const textChunk = child.nodeValue.trim();
                // If this is only whitespace, only add such a token and then visit the next node
                if (textChunk === "") {
                    tokens.push({
                        type: "whitespace",
                        textRepresentation: " ",
                    });
                    continue;
                }
                // Otherwise, parse the text content and add whitespace + word tokens as necessary
                const leadingSpace = /^\s+/.exec(child.nodeValue);
                const trailingSpace = /\s+$/.exec(child.nodeValue);
                if (leadingSpace !== null) {
                    tokens.push({
                        type: "whitespace",
                        textRepresentation: leadingSpace[0],
                    });
                }
                const words = textChunk.split(" ");
                words.forEach((word, wordIndex) => {
                    // Don't add empty words
                    if (word !== "") {
                        tokens.push({
                            type: "word",
                            textRepresentation: word,
                        });
                    }
                    // Add whitespace tokens for spaces in between words, eg not after the last word
                    if (wordIndex !== words.length - 1) {
                        tokens.push({
                            type: "whitespace",
                            textRepresentation: " ",
                        });
                    }
                });
                if (trailingSpace !== null) {
                    tokens.push({
                        type: "whitespace",
                        textRepresentation: trailingSpace[0],
                    });
                }
            }
            else {
                const startTagMatch = /^<[^>]*>/gm.exec(child.outerHTML);
                const endTagMatch = /<\/[^>]*>$/gm.exec(child.outerHTML);
                const tagName = child.tagName.toLowerCase();
                tokens.push({
                    type: "tag",
                    tagName,
                    textRepresentation: startTagMatch[0],
                });
                const childTokens = serializeNodeIntoTokens(child);
                tokens.push(...childTokens);
                if (endTagMatch) {
                    tokens.push({
                        type: "tag",
                        tagName,
                        textRepresentation: endTagMatch[0],
                    });
                }
            }
        }
    }
    catch (err) {
        console.error(err);
        throw err;
    }
    return tokens;
}
const project = (detaggedString, translatedString) => {
    // console.info("project", { detaggedString, translatedString });
    // Return the translated string as is if there were no tokens in the original string
    if (detaggedString.tokens.filter(token => token.type === "tag").length === 0) {
        return translatedString;
    }
    // If the last token is a tag, we pop it off the token array and re-add it
    // last so that the last tag gets any additional translation content injected into it
    const lastToken = detaggedString.tokens.slice(-1)[0];
    if (lastToken.type === "tag") {
        detaggedString.tokens.pop();
    }
    // Inject the tags naively in the translated string assuming a 1:1
    // relationship between original text nodes and words in the translated string
    const translatedStringWords = translatedString.split(" ");
    const remainingTranslatedStringWords = [...translatedStringWords];
    let whitespaceHaveBeenInjectedSinceTheLastWordWasInjected = true;
    const projectedStringParts = detaggedString.tokens.map((token) => {
        const determineProjectedStringPart = () => {
            if (token.type === "word") {
                const correspondingTranslatedWord = remainingTranslatedStringWords.shift();
                // If we have run out of translated words, don't attempt to add to the projected string
                if (correspondingTranslatedWord === undefined) {
                    return "";
                }
                // Otherwise, inject the translated word
                // ... possibly with a space injected in case none has been injected since the last word
                const $projectedStringPart = `${whitespaceHaveBeenInjectedSinceTheLastWordWasInjected ? "" : " "}${correspondingTranslatedWord}`;
                whitespaceHaveBeenInjectedSinceTheLastWordWasInjected = false;
                return $projectedStringPart;
            }
            else if (token.type === "whitespace") {
                // Don't pad whitespace onto each other when there are no more words
                if (remainingTranslatedStringWords.length === 0) {
                    return "";
                }
                whitespaceHaveBeenInjectedSinceTheLastWordWasInjected = true;
                return token.textRepresentation;
            }
            else if (token.type === "tag") {
                if (isTagTokenWithImpliedWhitespace(token)) {
                    whitespaceHaveBeenInjectedSinceTheLastWordWasInjected = true;
                }
                return token.textRepresentation;
            }
            throw new Error(`Unexpected token type: ${token.type}`);
        };
        const projectedStringPart = determineProjectedStringPart();
        return projectedStringPart;
    });
    let projectedString = projectedStringParts.join("");
    // Add any remaining translated words to the end
    if (remainingTranslatedStringWords.length) {
        // Add a whitespace to the end first in case there was none, or else two words will be joined together
        if (lastToken.type !== "whitespace") {
            projectedString += " ";
        }
        projectedString += remainingTranslatedStringWords.join(" ");
    }
    // If the last token is a tag, see above
    if (lastToken.type === "tag") {
        projectedString += lastToken.textRepresentation;
    }
    // console.debug({translatedStringWords, projectedString});
    return projectedString;
};
exports.project = project;


/***/ }),

/***/ 5173:
/*!**********************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/getTranslationNodes.ts ***!
  \**********************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.getTranslationNodes = void 0;
const hasTextForTranslation_1 = __webpack_require__(/*! ./hasTextForTranslation */ 4874);
const isBlockFrameOrSubclass = (element) => {
    // TODO: Make this generalize like the corresponding C code invoked by:
    /*
    nsIFrame* frame = childElement->GetPrimaryFrame();
    frame->IsBlockFrameOrSubclass();
    */
    const nodeTagName = element.tagName.toLowerCase();
    const blockLevelElementTagNames = [
        "address",
        "article",
        "aside",
        "blockquote",
        "canvas",
        "dd",
        "div",
        "dl",
        "dt",
        "fieldset",
        "figcaption",
        "figure",
        "footer",
        "form",
        "h1",
        "h2",
        "h3",
        "h4",
        "h5",
        "h6",
        "header",
        "hr",
        "li",
        "main",
        "nav",
        "noscript",
        "ol",
        "p",
        "pre",
        "section",
        "table",
        "tfoot",
        "ul",
        "video",
    ];
    const result = (blockLevelElementTagNames.includes(nodeTagName) ||
        element.style.display === "block") &&
        element.style.display !== "inline";
    return result;
};
const getTranslationNodes = (rootElement, seenTranslationNodes = [], limit = 15000) => {
    const translationNodes = [];
    // Query child elements in order to explicitly skip the root element from being classified as a translation node
    const childElements = rootElement.children;
    for (let i = 0; i < limit && i < childElements.length; i++) {
        const childElement = childElements[i];
        const tagName = childElement.tagName.toLowerCase();
        const isTextNode = childElement.nodeType === Node.TEXT_NODE;
        if (isTextNode) {
            console.warn(`We are not supposed to run into text nodes here. childElement.textContent: "${childElement.textContent}"`);
            continue;
        }
        // Skip elements that usually contain non-translatable text content.
        if ([
            "script",
            "iframe",
            "frameset",
            "frame",
            "code",
            "noscript",
            "style",
            "svg",
            "math",
        ].includes(tagName)) {
            continue;
        }
        const nodeHasTextForTranslation = hasTextForTranslation_1.hasTextForTranslation(childElement.textContent);
        // Only empty or non-translatable content in this part of the tree
        if (!nodeHasTextForTranslation) {
            continue;
        }
        // An element is a translation node if it contains
        // at least one text node that has meaningful data
        // for translation
        const childChildTextNodes = Array.from(childElement.childNodes).filter((childChildNode) => childChildNode.nodeType === Node.TEXT_NODE);
        const childChildTextNodesWithTextForTranslation = childChildTextNodes
            .map(textNode => textNode.textContent)
            .filter(hasTextForTranslation_1.hasTextForTranslation);
        const isTranslationNode = !!childChildTextNodesWithTextForTranslation.length;
        if (isTranslationNode) {
            // At this point, we know we have a translation node at hand, but we need
            // to figure out it the node is a translation root or not
            let isTranslationRoot;
            // Block elements are translation roots
            isTranslationRoot = isBlockFrameOrSubclass(childElement);
            // If an element is not a block element, it still
            // can be considered a translation root if all ancestors
            // of this element are not translation nodes
            seenTranslationNodes.push(childElement);
            if (!isTranslationRoot) {
                // Walk up tree and check if an ancestor was a translation node
                let ancestorWasATranslationNode = false;
                for (let ancestor = childElement.parentNode; ancestor; ancestor = ancestor.parentNode) {
                    if (seenTranslationNodes.includes(ancestor)) {
                        ancestorWasATranslationNode = true;
                        break;
                    }
                }
                isTranslationRoot = !ancestorWasATranslationNode;
            }
            const translationNode = {
                content: childElement,
                isTranslationRoot,
            };
            translationNodes.push(translationNode);
        }
        // Now traverse any element children to find nested translation nodes
        if (childElement.firstElementChild) {
            const childTranslationNodes = exports.getTranslationNodes(childElement, seenTranslationNodes, limit - translationNodes.length);
            translationNodes.push(...childTranslationNodes);
        }
    }
    return translationNodes;
};
exports.getTranslationNodes = getTranslationNodes;


/***/ }),

/***/ 4874:
/*!************************************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/hasTextForTranslation.ts ***!
  \************************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.hasTextForTranslation = void 0;
const hasTextForTranslation = text => {
    const trimmed = text.trim();
    if (trimmed === "") {
        return false;
    }
    /* tslint:disable:no-empty-character-class */
    // https://github.com/buzinas/tslint-eslint-rules/issues/289
    return /\p{L}/gu.test(trimmed);
};
exports.hasTextForTranslation = hasTextForTranslation;


/***/ }),

/***/ 5543:
/*!********************************************************************************!*\
  !*** ./src/core/ts/content-scripts/dom-translation-content-script.js/index.ts ***!
  \********************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const DomTranslationManager_1 = __webpack_require__(/*! ./DomTranslationManager */ 9023);
const subscribeToExtensionState_1 = __webpack_require__(/*! ../../shared-resources/state-management/subscribeToExtensionState */ 6523);
const DocumentTranslationStateCommunicator_1 = __webpack_require__(/*! ../../shared-resources/state-management/DocumentTranslationStateCommunicator */ 2187);
const ContentScriptFrameInfo_1 = __webpack_require__(/*! ../../shared-resources/ContentScriptFrameInfo */ 9181);
const ExtensionState_1 = __webpack_require__(/*! ../../shared-resources/models/ExtensionState */ 65);
const BaseTranslationState_1 = __webpack_require__(/*! ../../shared-resources/models/BaseTranslationState */ 4779);
const TranslateOwnTextTranslationState_1 = __webpack_require__(/*! ../../shared-resources/models/TranslateOwnTextTranslationState */ 8238);
const DocumentTranslationState_1 = __webpack_require__(/*! ../../shared-resources/models/DocumentTranslationState */ 5482);
// Workaround for https://github.com/xaviergonz/mobx-keystone/issues/183
// We need to import some models explicitly lest they fail to be registered by mobx
new ExtensionState_1.ExtensionState({});
new TranslateOwnTextTranslationState_1.TranslateOwnTextTranslationState({});
/**
 * Note this content script runs at "document_idle" ie after the DOM is complete
 */
const init = () => __awaiter(void 0, void 0, void 0, function* () {
    /*
    await initErrorReportingInContentScript(
      "port-from-dom-translation-content-script:index",
    );
    */
    const contentScriptFrameInfo = new ContentScriptFrameInfo_1.ContentScriptFrameInfo();
    // Get window, tab and frame id
    const frameInfo = yield contentScriptFrameInfo.getCurrentFrameInfo();
    const tabFrameReference = `${frameInfo.tabId}-${frameInfo.frameId}`;
    const extensionState = yield subscribeToExtensionState_1.subscribeToExtensionState();
    const documentTranslationStateCommunicator = new DocumentTranslationStateCommunicator_1.DocumentTranslationStateCommunicator(frameInfo, extensionState);
    const domTranslationManager = new DomTranslationManager_1.DomTranslationManager(documentTranslationStateCommunicator, document, window);
    const documentTranslationStatistics = yield domTranslationManager.getDocumentTranslationStatistics();
    console.log({ documentTranslationStatistics });
    // TODO: Prevent multiple translations from occurring simultaneously + enable cancellations of existing translation jobs
    // Any subsequent actions are determined by document translation state changes
    mobx_keystone_1.onSnapshot(extensionState.$.documentTranslationStates, (documentTranslationStates, previousDocumentTranslationStates) => __awaiter(void 0, void 0, void 0, function* () {
        // console.debug("dom-translation-content-script.js - documentTranslationStates snapshot HAS CHANGED", {documentTranslationStates});
        var _a, _b;
        const currentTabFrameDocumentTranslationState = documentTranslationStates[tabFrameReference];
        const previousTabFrameDocumentTranslationState = previousDocumentTranslationStates[tabFrameReference];
        // console.log({ currentTabFrameDocumentTranslationState });
        // TODO: Possibly react to no current state in some other way
        if (!currentTabFrameDocumentTranslationState) {
            return;
        }
        const hasChanged = property => {
            return (!previousTabFrameDocumentTranslationState ||
                currentTabFrameDocumentTranslationState[property] !==
                    previousTabFrameDocumentTranslationState[property]);
        };
        if (hasChanged("translationRequested")) {
            if (currentTabFrameDocumentTranslationState.translationRequested) {
                /* TODO: Do not translate if already translated
              if (
                domTranslationManager?.contentWindow?.translationDocument &&
                currentTabFrameDocumentTranslationState.translateFrom !==
                  domTranslationManager.contentWindow.translationDocument.sourceLanguage
              ) {
                */
                const translationPromise = domTranslationManager.doTranslation(currentTabFrameDocumentTranslationState.translateFrom, currentTabFrameDocumentTranslationState.translateTo);
                extensionState.patchDocumentTranslationStateByFrameInfo(frameInfo, [
                    {
                        op: "replace",
                        path: ["translationRequested"],
                        value: false,
                    },
                ]);
                yield translationPromise;
            }
        }
        if (hasChanged("translationStatus")) {
            if (currentTabFrameDocumentTranslationState.translationStatus ===
                BaseTranslationState_1.TranslationStatus.UNKNOWN) {
                yield domTranslationManager.attemptToDetectLanguage();
            }
            if (currentTabFrameDocumentTranslationState.translationStatus ===
                BaseTranslationState_1.TranslationStatus.TRANSLATING) {
                if (currentTabFrameDocumentTranslationState.cancellationRequested) {
                    console.debug("Cancellation requested");
                    console.debug("TODO: Implement");
                }
            }
        }
        if ((_a = domTranslationManager === null || domTranslationManager === void 0 ? void 0 : domTranslationManager.contentWindow) === null || _a === void 0 ? void 0 : _a.translationDocument) {
            const translationDocument = (_b = domTranslationManager === null || domTranslationManager === void 0 ? void 0 : domTranslationManager.contentWindow) === null || _b === void 0 ? void 0 : _b.translationDocument;
            if (hasChanged("showOriginal")) {
                if (currentTabFrameDocumentTranslationState.showOriginal !==
                    translationDocument.originalShown) {
                    if (translationDocument.originalShown) {
                        translationDocument.showTranslation();
                    }
                    else {
                        translationDocument.showOriginal();
                    }
                }
            }
            if (hasChanged("displayQualityEstimation")) {
                if (currentTabFrameDocumentTranslationState.displayQualityEstimation !==
                    translationDocument.qualityEstimationShown) {
                    if (translationDocument.qualityEstimationShown) {
                        translationDocument.showTranslation();
                    }
                    else {
                        translationDocument.showQualityEstimation();
                    }
                }
            }
        }
    }));
    // Add an initial document translation state
    try {
        extensionState.setDocumentTranslationState(new DocumentTranslationState_1.DocumentTranslationState(Object.assign(Object.assign({}, frameInfo), { translationStatus: BaseTranslationState_1.TranslationStatus.UNKNOWN, url: window.location.href, wordCount: documentTranslationStatistics.wordCount, wordCountVisible: documentTranslationStatistics.wordCountVisible, wordCountVisibleInViewport: documentTranslationStatistics.wordCountVisibleInViewport })));
        // Schedule removal of this document translation state when the document is closed
        const onBeforeunloadEventListener = function (e) {
            extensionState.deleteDocumentTranslationStateByFrameInfo(frameInfo);
            // the absence of a returnValue property on the event will guarantee the browser unload happens
            delete e.returnValue;
            // balanced-listeners
            window.removeEventListener("beforeunload", onBeforeunloadEventListener);
        };
        window.addEventListener("beforeunload", onBeforeunloadEventListener);
    }
    catch (err) {
        console.error("Instantiate DocumentTranslationState error", err);
    }
});
// noinspection JSIgnoredPromiseFromCall
init();


/***/ }),

/***/ 449:
/*!************************************************************************!*\
  !*** ./src/core/ts/shared-resources/ContentScriptBergamotApiClient.ts ***!
  \************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.ContentScriptBergamotApiClient = void 0;
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const nanoid_1 = __webpack_require__(/*! nanoid */ 350);
const ErrorReporting_1 = __webpack_require__(/*! ./ErrorReporting */ 3345);
class ContentScriptBergamotApiClient {
    constructor() {
        // console.debug("ContentScriptBergamotApiClient: Connecting to the background script");
        this.backgroundContextPort = webextension_polyfill_ts_1.browser.runtime.connect(webextension_polyfill_ts_1.browser.runtime.id, {
            name: "port-from-content-script-bergamot-api-client",
        });
    }
    sendTranslationRequest(texts, from, to, translationRequestProgressCallback) {
        return __awaiter(this, void 0, void 0, function* () {
            return new Promise((resolve, reject) => {
                const requestId = nanoid_1.nanoid();
                const resultsMessageListener = (m) => __awaiter(this, void 0, void 0, function* () {
                    if (m.translationRequestUpdate) {
                        const { translationRequestUpdate } = m;
                        if (translationRequestUpdate.requestId !== requestId) {
                            return;
                        }
                        // console.debug("ContentScriptBergamotApiClient received translationRequestUpdate", { translationRequestUpdate });
                        const { results, translationRequestProgress, error, } = translationRequestUpdate;
                        if (translationRequestProgress) {
                            translationRequestProgressCallback(translationRequestProgress);
                            return;
                        }
                        if (results) {
                            this.backgroundContextPort.onMessage.removeListener(resultsMessageListener);
                            resolve(translationRequestUpdate.results);
                            return;
                        }
                        if (error) {
                            this.backgroundContextPort.onMessage.removeListener(resultsMessageListener);
                            reject(error);
                            return;
                        }
                    }
                    ErrorReporting_1.captureExceptionWithExtras(new Error("Unexpected message structure"), {
                        m,
                    });
                    console.error("Unexpected message structure", { m });
                    reject({ m });
                });
                this.backgroundContextPort.onMessage.addListener(resultsMessageListener);
                // console.debug("ContentScriptBergamotApiClient: Sending translation request", {texts});
                this.backgroundContextPort.postMessage({
                    texts,
                    from,
                    to,
                    requestId,
                });
            });
        });
    }
}
exports.ContentScriptBergamotApiClient = ContentScriptBergamotApiClient;


/***/ }),

/***/ 9181:
/*!****************************************************************!*\
  !*** ./src/core/ts/shared-resources/ContentScriptFrameInfo.ts ***!
  \****************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.ContentScriptFrameInfo = void 0;
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const ErrorReporting_1 = __webpack_require__(/*! ./ErrorReporting */ 3345);
const nanoid_1 = __webpack_require__(/*! nanoid */ 350);
class ContentScriptFrameInfo {
    constructor() {
        // console.debug("ContentScriptFrameInfo: Connecting to the background script");
        this.backgroundContextPort = webextension_polyfill_ts_1.browser.runtime.connect(webextension_polyfill_ts_1.browser.runtime.id, {
            name: "port-from-content-script-frame-info",
        });
    }
    getCurrentFrameInfo() {
        return __awaiter(this, void 0, void 0, function* () {
            return new Promise((resolve, reject) => {
                const requestId = nanoid_1.nanoid();
                const resultsMessageListener = (m) => __awaiter(this, void 0, void 0, function* () {
                    if (m.frameInfo) {
                        const { frameInfo } = m;
                        if (m.requestId !== requestId) {
                            return;
                        }
                        // console.debug("ContentScriptFrameInfo received results", {frameInfo});
                        this.backgroundContextPort.onMessage.removeListener(resultsMessageListener);
                        resolve(frameInfo);
                        return;
                    }
                    ErrorReporting_1.captureExceptionWithExtras(new Error("Unexpected message"), { m });
                    console.error("Unexpected message", { m });
                    reject({ m });
                });
                this.backgroundContextPort.onMessage.addListener(resultsMessageListener);
                this.backgroundContextPort.postMessage({
                    requestId,
                });
            });
        });
    }
}
exports.ContentScriptFrameInfo = ContentScriptFrameInfo;


/***/ }),

/***/ 6336:
/*!****************************************************************************!*\
  !*** ./src/core/ts/shared-resources/ContentScriptLanguageDetectorProxy.ts ***!
  \****************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.ContentScriptLanguageDetectorProxy = void 0;
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const ErrorReporting_1 = __webpack_require__(/*! ./ErrorReporting */ 3345);
const nanoid_1 = __webpack_require__(/*! nanoid */ 350);
class ContentScriptLanguageDetectorProxy {
    constructor() {
        // console.debug("ContentScriptLanguageDetectorProxy: Connecting to the background script");
        this.backgroundContextPort = webextension_polyfill_ts_1.browser.runtime.connect(webextension_polyfill_ts_1.browser.runtime.id, {
            name: "port-from-content-script-language-detector-proxy",
        });
    }
    detectLanguage(str) {
        return __awaiter(this, void 0, void 0, function* () {
            return new Promise((resolve, reject) => {
                const requestId = nanoid_1.nanoid();
                const resultsMessageListener = (m) => __awaiter(this, void 0, void 0, function* () {
                    if (m.languageDetectorResults) {
                        const { languageDetectorResults } = m;
                        if (languageDetectorResults.requestId !== requestId) {
                            return;
                        }
                        // console.debug("ContentScriptLanguageDetectorProxy received language detector results", {languageDetectorResults});
                        this.backgroundContextPort.onMessage.removeListener(resultsMessageListener);
                        resolve(languageDetectorResults.results);
                        return;
                    }
                    ErrorReporting_1.captureExceptionWithExtras(new Error("Unexpected message"), { m });
                    console.error("Unexpected message", { m });
                    reject({ m });
                });
                this.backgroundContextPort.onMessage.addListener(resultsMessageListener);
                // console.debug("Attempting detectLanguage via content script proxy", {str});
                this.backgroundContextPort.postMessage({
                    str,
                    requestId,
                });
            });
        });
    }
}
exports.ContentScriptLanguageDetectorProxy = ContentScriptLanguageDetectorProxy;


/***/ }),

/***/ 2187:
/*!***********************************************************************************************!*\
  !*** ./src/core/ts/shared-resources/state-management/DocumentTranslationStateCommunicator.ts ***!
  \***********************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.DocumentTranslationStateCommunicator = void 0;
/**
 * Helper class to communicate updated document translation states.
 *
 * State patching code is wrapped in setTimeout to prevent
 * automatic (by mobx) batching of updates which leads to much less frequent
 * state updates communicated to subscribers. No state updates during a translation
 * session is not useful since we want to communicate the translation progress)
 */
class DocumentTranslationStateCommunicator {
    constructor(frameInfo, extensionState) {
        this.frameInfo = frameInfo;
        this.extensionState = extensionState;
    }
    patchDocumentTranslationState(patches) {
        setTimeout(() => {
            this.extensionState.patchDocumentTranslationStateByFrameInfo(this.frameInfo, patches);
        }, 0);
    }
    broadcastUpdatedAttributeValue(attribute, value) {
        this.patchDocumentTranslationState([
            {
                op: "replace",
                path: [attribute],
                value,
            },
        ]);
    }
    broadcastUpdatedTranslationStatus(translationStatus) {
        this.broadcastUpdatedAttributeValue("translationStatus", translationStatus);
    }
    /**
     * This method was chosen as the place to sum up the progress of individual translation
     * requests into the similar translation progress attributes present at the frame level
     * in document translation state objects (totalTranslationWallTimeMs, totalTranslationEngineRequestCount etc).
     *
     * Another natural place to do this conversion would be as computed properties in the mobx models
     * but it proved problematic to maintain/patch/sync map attributes (such as progressOfIndividualTranslationRequests)
     * in document translation state objects, so reduction to simpler attributes is done here instead.
     *
     * @param frameTranslationProgress
     */
    broadcastUpdatedFrameTranslationProgress(frameTranslationProgress) {
        const { progressOfIndividualTranslationRequests, } = frameTranslationProgress;
        const translationRequestProgressEntries = Array.from(progressOfIndividualTranslationRequests).map(([, translationRequestProgress]) => translationRequestProgress);
        const translationInitiationTimestamps = translationRequestProgressEntries.map((trp) => trp.initiationTimestamp);
        const translationInitiationTimestamp = Math.min(...translationInitiationTimestamps);
        const totalModelLoadWallTimeMs = translationRequestProgressEntries
            .map((trp) => trp.modelLoadWallTimeMs || 0)
            .reduce((a, b) => a + b, 0);
        const totalTranslationWallTimeMs = translationRequestProgressEntries
            .map((trp) => trp.translationWallTimeMs || 0)
            .reduce((a, b) => a + b, 0);
        const totalTranslationEngineRequestCount = translationRequestProgressEntries.length;
        const queuedTranslationEngineRequestCount = translationRequestProgressEntries.filter((trp) => trp.queued).length;
        // Merge translation-progress-related booleans
        const modelLoadNecessary = !!translationRequestProgressEntries.filter((trp) => trp.modelLoadNecessary).length;
        const modelDownloadNecessary = !!translationRequestProgressEntries.filter((trp) => trp.modelDownloadNecessary).length;
        const modelDownloading = !!translationRequestProgressEntries.filter((trp) => trp.modelDownloading).length;
        const modelLoading = modelLoadNecessary
            ? !!translationRequestProgressEntries.find((trp) => trp.modelLoading)
            : undefined;
        const modelLoaded = modelLoadNecessary
            ? !!translationRequestProgressEntries
                .filter((trp) => trp.modelLoadNecessary)
                .find((trp) => trp.modelLoaded)
            : undefined;
        const translationFinished = translationRequestProgressEntries.filter((trp) => !trp.translationFinished).length === 0;
        // Merge model download progress
        const emptyDownloadProgress = {
            bytesDownloaded: 0,
            bytesToDownload: 0,
            startTs: undefined,
            durationMs: 0,
            endTs: undefined,
        };
        const modelDownloadProgress = translationRequestProgressEntries
            .map((trp) => trp.modelDownloadProgress)
            .filter((mdp) => mdp)
            .reduce((a, b) => {
            const startTs = a.startTs && a.startTs <= b.startTs ? a.startTs : b.startTs;
            const endTs = a.endTs && a.endTs >= b.endTs ? a.endTs : b.endTs;
            return {
                bytesDownloaded: a.bytesDownloaded + b.bytesDownloaded,
                bytesToDownload: a.bytesToDownload + b.bytesToDownload,
                startTs,
                durationMs: endTs ? endTs - startTs : Date.now() - startTs,
                endTs,
            };
        }, emptyDownloadProgress);
        this.patchDocumentTranslationState([
            {
                op: "replace",
                path: ["translationInitiationTimestamp"],
                value: translationInitiationTimestamp,
            },
            {
                op: "replace",
                path: ["totalModelLoadWallTimeMs"],
                value: totalModelLoadWallTimeMs,
            },
            {
                op: "replace",
                path: ["modelDownloadNecessary"],
                value: modelDownloadNecessary,
            },
            {
                op: "replace",
                path: ["modelDownloading"],
                value: modelDownloading,
            },
            {
                op: "replace",
                path: ["modelDownloadProgress"],
                value: modelDownloadProgress,
            },
            {
                op: "replace",
                path: ["totalTranslationWallTimeMs"],
                value: totalTranslationWallTimeMs,
            },
            {
                op: "replace",
                path: ["totalTranslationEngineRequestCount"],
                value: totalTranslationEngineRequestCount,
            },
            {
                op: "replace",
                path: ["queuedTranslationEngineRequestCount"],
                value: queuedTranslationEngineRequestCount,
            },
            {
                op: "replace",
                path: ["modelLoadNecessary"],
                value: modelLoadNecessary,
            },
            {
                op: "replace",
                path: ["modelLoading"],
                value: modelLoading,
            },
            {
                op: "replace",
                path: ["modelLoaded"],
                value: modelLoaded,
            },
            {
                op: "replace",
                path: ["translationFinished"],
                value: translationFinished,
            },
        ]);
    }
    clear() {
        setTimeout(() => {
            this.extensionState.deleteDocumentTranslationStateByFrameInfo(this.frameInfo);
        }, 0);
    }
    updatedDetectedLanguageResults(detectedLanguageResults) {
        this.broadcastUpdatedAttributeValue("detectedLanguageResults", detectedLanguageResults);
    }
}
exports.DocumentTranslationStateCommunicator = DocumentTranslationStateCommunicator;


/***/ }),

/***/ 6523:
/*!************************************************************************************!*\
  !*** ./src/core/ts/shared-resources/state-management/subscribeToExtensionState.ts ***!
  \************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.subscribeToExtensionState = void 0;
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const ErrorReporting_1 = __webpack_require__(/*! ../ErrorReporting */ 3345);
const nanoid_1 = __webpack_require__(/*! nanoid */ 350);
// disable runtime data checking (we rely on TypeScript at compile time so that our model definitions can be cleaner)
mobx_keystone_1.setGlobalConfig({
    modelAutoTypeChecking: mobx_keystone_1.ModelAutoTypeCheckingMode.AlwaysOff,
});
class MobxKeystoneProxy {
    constructor(msgListeners) {
        this.msgListeners = [];
        this.msgListeners = msgListeners;
        // console.debug("MobxKeystoneProxy: Connecting to the background script");
        this.backgroundContextPort = webextension_polyfill_ts_1.browser.runtime.connect(webextension_polyfill_ts_1.browser.runtime.id, {
            name: "port-from-mobx-keystone-proxy",
        });
        // listen to updates from host
        const actionCallResultsMessageListener = (m) => __awaiter(this, void 0, void 0, function* () {
            if (m.serializedActionCallToReplicate) {
                const { serializedActionCallToReplicate } = m;
                // console.log("MobxKeystoneProxy received applyActionResult", {serializedActionCallToReplicate});
                this.msgListeners.forEach(listener => listener(serializedActionCallToReplicate));
                return;
            }
            if (m.initialState) {
                // handled in another listener, do nothing here
                return;
            }
            ErrorReporting_1.captureExceptionWithExtras(new Error("Unexpected message"), { m });
            console.error("Unexpected message", { m });
        });
        this.backgroundContextPort.onMessage.addListener(actionCallResultsMessageListener);
    }
    requestInitialState() {
        return __awaiter(this, void 0, void 0, function* () {
            return new Promise((resolve, reject) => {
                const requestId = nanoid_1.nanoid();
                const resultsMessageListener = (m) => __awaiter(this, void 0, void 0, function* () {
                    if (m.initialState) {
                        const { initialState } = m;
                        if (m.requestId !== requestId) {
                            return;
                        }
                        // console.debug("MobxKeystoneProxy received initialState", {initialState});
                        this.backgroundContextPort.onMessage.removeListener(resultsMessageListener);
                        resolve(initialState);
                        return;
                    }
                    if (m.serializedActionCallToReplicate) {
                        // handled in another listener, do nothing here
                        return;
                    }
                    ErrorReporting_1.captureExceptionWithExtras(new Error("Unexpected message"), { m });
                    console.error("Unexpected message", { m });
                    reject({ m });
                });
                this.backgroundContextPort.onMessage.addListener(resultsMessageListener);
                // console.debug("requestInitialState via content script mobx keystone proxy", {});
                this.backgroundContextPort.postMessage({
                    requestInitialState: true,
                    requestId,
                });
            });
        });
    }
    actionCall(actionCall) {
        return __awaiter(this, void 0, void 0, function* () {
            return new Promise((resolve, _reject) => {
                const requestId = nanoid_1.nanoid();
                // console.debug("MobxKeystoneProxy (Content Script Context): actionCall via content script mobx keystone proxy", { actionCall });
                this.backgroundContextPort.postMessage({
                    actionCall,
                    requestId,
                });
                resolve();
            });
        });
    }
}
class BackgroundContextCommunicator {
    constructor() {
        this.msgListeners = [];
        this.mobxKeystoneProxy = new MobxKeystoneProxy(this.msgListeners);
    }
    requestInitialState() {
        return __awaiter(this, void 0, void 0, function* () {
            return this.mobxKeystoneProxy.requestInitialState();
        });
    }
    onMessage(listener) {
        this.msgListeners.push(listener);
    }
    sendMessage(actionCall) {
        // send the action to be taken to the host
        this.mobxKeystoneProxy.actionCall(actionCall);
    }
}
const server = new BackgroundContextCommunicator();
function subscribeToExtensionState() {
    return __awaiter(this, void 0, void 0, function* () {
        // we get the snapshot from the server, which is a serializable object
        const rootStoreSnapshot = yield server.requestInitialState();
        // and hydrate it into a proper object
        const rootStore = mobx_keystone_1.fromSnapshot(rootStoreSnapshot);
        let serverAction = false;
        const runServerActionLocally = (actionCall) => {
            const wasServerAction = serverAction;
            serverAction = true;
            try {
                // in clients we use the sync new model ids version to make sure that
                // any model ids that were generated in the server side end up being
                // the same in the client side
                mobx_keystone_1.applySerializedActionAndSyncNewModelIds(rootStore, actionCall);
            }
            finally {
                serverAction = wasServerAction;
            }
        };
        // listen to action messages to be replicated into the local root store
        server.onMessage(actionCall => {
            runServerActionLocally(actionCall);
        });
        // also listen to local actions, cancel them and send them to the server (background context)
        mobx_keystone_1.onActionMiddleware(rootStore, {
            onStart(actionCall, ctx) {
                if (!serverAction) {
                    // if the action does not come from the server (background context) cancel it silently
                    // and send it to the server (background context)
                    // it will then be replicated by the server (background context) and properly executed
                    server.sendMessage(mobx_keystone_1.serializeActionCall(actionCall, rootStore));
                    ctx.data.cancelled = true; // just for logging purposes
                    // "cancel" the action by returning undefined
                    return {
                        result: mobx_keystone_1.ActionTrackingResult.Return,
                        value: undefined,
                    };
                }
                // run actions that comes from the server (background context) unmodified
                /* eslint-disable consistent-return */
                return undefined;
                /* eslint-enable consistent-return */
            },
        });
        // recommended by mobx-keystone (allows the model hook `onAttachedToRootStore` to work)
        mobx_keystone_1.registerRootStore(rootStore);
        return rootStore;
    });
}
exports.subscribeToExtensionState = subscribeToExtensionState;


/***/ })

/******/ 	});
/************************************************************************/
/******/ 	// The module cache
/******/ 	var __webpack_module_cache__ = {};
/******/ 	
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/ 		// Check if module is in cache
/******/ 		if(__webpack_module_cache__[moduleId]) {
/******/ 			return __webpack_module_cache__[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = __webpack_module_cache__[moduleId] = {
/******/ 			id: moduleId,
/******/ 			loaded: false,
/******/ 			exports: {}
/******/ 		};
/******/ 	
/******/ 		// Execute the module function
/******/ 		__webpack_modules__[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/ 	
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
/******/ 	
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/ 	
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = __webpack_modules__;
/******/ 	
/******/ 	// the startup function
/******/ 	// It's empty as some runtime module handles the default behavior
/******/ 	__webpack_require__.x = x => {};
/************************************************************************/
/******/ 	/* webpack/runtime/compat get default export */
/******/ 	(() => {
/******/ 		// getDefaultExport function for compatibility with non-harmony modules
/******/ 		__webpack_require__.n = (module) => {
/******/ 			var getter = module && module.__esModule ?
/******/ 				() => (module['default']) :
/******/ 				() => (module);
/******/ 			__webpack_require__.d(getter, { a: getter });
/******/ 			return getter;
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/define property getters */
/******/ 	(() => {
/******/ 		// define getter functions for harmony exports
/******/ 		__webpack_require__.d = (exports, definition) => {
/******/ 			for(var key in definition) {
/******/ 				if(__webpack_require__.o(definition, key) && !__webpack_require__.o(exports, key)) {
/******/ 					Object.defineProperty(exports, key, { enumerable: true, get: definition[key] });
/******/ 				}
/******/ 			}
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/global */
/******/ 	(() => {
/******/ 		__webpack_require__.g = (function() {
/******/ 			if (typeof globalThis === 'object') return globalThis;
/******/ 			try {
/******/ 				return this || new Function('return this')();
/******/ 			} catch (e) {
/******/ 				if (typeof window === 'object') return window;
/******/ 			}
/******/ 		})();
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/harmony module decorator */
/******/ 	(() => {
/******/ 		__webpack_require__.hmd = (module) => {
/******/ 			module = Object.create(module);
/******/ 			if (!module.children) module.children = [];
/******/ 			Object.defineProperty(module, 'exports', {
/******/ 				enumerable: true,
/******/ 				set: () => {
/******/ 					throw new Error('ES Modules may not assign module.exports or exports.*, Use ESM export syntax, instead: ' + module.id);
/******/ 				}
/******/ 			});
/******/ 			return module;
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/hasOwnProperty shorthand */
/******/ 	(() => {
/******/ 		__webpack_require__.o = (obj, prop) => (Object.prototype.hasOwnProperty.call(obj, prop))
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/make namespace object */
/******/ 	(() => {
/******/ 		// define __esModule on exports
/******/ 		__webpack_require__.r = (exports) => {
/******/ 			if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 				Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 			}
/******/ 			Object.defineProperty(exports, '__esModule', { value: true });
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/jsonp chunk loading */
/******/ 	(() => {
/******/ 		// no baseURI
/******/ 		
/******/ 		// object to store loaded and loading chunks
/******/ 		// undefined = chunk not loaded, null = chunk preloaded/prefetched
/******/ 		// Promise = chunk loading, 0 = chunk loaded
/******/ 		var installedChunks = {
/******/ 			840: 0
/******/ 		};
/******/ 		
/******/ 		var deferredModules = [
/******/ 			[5543,351]
/******/ 		];
/******/ 		// no chunk on demand loading
/******/ 		
/******/ 		// no prefetching
/******/ 		
/******/ 		// no preloaded
/******/ 		
/******/ 		// no HMR
/******/ 		
/******/ 		// no HMR manifest
/******/ 		
/******/ 		var checkDeferredModules = x => {};
/******/ 		
/******/ 		// install a JSONP callback for chunk loading
/******/ 		var webpackJsonpCallback = (parentChunkLoadingFunction, data) => {
/******/ 			var [chunkIds, moreModules, runtime, executeModules] = data;
/******/ 			// add "moreModules" to the modules object,
/******/ 			// then flag all "chunkIds" as loaded and fire callback
/******/ 			var moduleId, chunkId, i = 0, resolves = [];
/******/ 			for(;i < chunkIds.length; i++) {
/******/ 				chunkId = chunkIds[i];
/******/ 				if(__webpack_require__.o(installedChunks, chunkId) && installedChunks[chunkId]) {
/******/ 					resolves.push(installedChunks[chunkId][0]);
/******/ 				}
/******/ 				installedChunks[chunkId] = 0;
/******/ 			}
/******/ 			for(moduleId in moreModules) {
/******/ 				if(__webpack_require__.o(moreModules, moduleId)) {
/******/ 					__webpack_require__.m[moduleId] = moreModules[moduleId];
/******/ 				}
/******/ 			}
/******/ 			if(runtime) runtime(__webpack_require__);
/******/ 			if(parentChunkLoadingFunction) parentChunkLoadingFunction(data);
/******/ 			while(resolves.length) {
/******/ 				resolves.shift()();
/******/ 			}
/******/ 		
/******/ 			// add entry modules from loaded chunk to deferred list
/******/ 			if(executeModules) deferredModules.push.apply(deferredModules, executeModules);
/******/ 		
/******/ 			// run deferred modules when all chunks ready
/******/ 			return checkDeferredModules();
/******/ 		}
/******/ 		
/******/ 		var chunkLoadingGlobal = self["webpackChunkbergamot_browser_extension"] = self["webpackChunkbergamot_browser_extension"] || [];
/******/ 		chunkLoadingGlobal.forEach(webpackJsonpCallback.bind(null, 0));
/******/ 		chunkLoadingGlobal.push = webpackJsonpCallback.bind(null, chunkLoadingGlobal.push.bind(chunkLoadingGlobal));
/******/ 		
/******/ 		function checkDeferredModulesImpl() {
/******/ 			var result;
/******/ 			for(var i = 0; i < deferredModules.length; i++) {
/******/ 				var deferredModule = deferredModules[i];
/******/ 				var fulfilled = true;
/******/ 				for(var j = 1; j < deferredModule.length; j++) {
/******/ 					var depId = deferredModule[j];
/******/ 					if(installedChunks[depId] !== 0) fulfilled = false;
/******/ 				}
/******/ 				if(fulfilled) {
/******/ 					deferredModules.splice(i--, 1);
/******/ 					result = __webpack_require__(__webpack_require__.s = deferredModule[0]);
/******/ 				}
/******/ 			}
/******/ 			if(deferredModules.length === 0) {
/******/ 				__webpack_require__.x();
/******/ 				__webpack_require__.x = x => {};
/******/ 			}
/******/ 			return result;
/******/ 		}
/******/ 		var startup = __webpack_require__.x;
/******/ 		__webpack_require__.x = () => {
/******/ 			// reset startup function so it can be called again when more startup code is added
/******/ 			__webpack_require__.x = startup || (x => {});
/******/ 			return (checkDeferredModules = checkDeferredModulesImpl)();
/******/ 		};
/******/ 	})();
/******/ 	
/************************************************************************/
/******/ 	
/******/ 	// run startup
/******/ 	var __webpack_exports__ = __webpack_require__.x();
/******/ 	
/******/ })()
;
//# sourceMappingURL=dom-translation-content-script.js.map