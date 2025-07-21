export namespace Readerable {
    const isEnabledForParseOnLoad: any;
    /**
     * Decides whether or not a document is reader-able without parsing the whole thing.
     *
     * @param doc A document to parse.
     * @return boolean Whether or not we should show the reader mode button.
     */
    function isProbablyReaderable(doc: any): boolean;
    function _isNodeVisible(node: any): boolean;
    let _blockedHosts: string[];
    function shouldCheckUri(uri: any, isBaseUri?: boolean): boolean;
}
/**
 * Decides whether or not the document is reader-able without parsing the whole thing.
 * @param {Object} options Configuration object.
 * @param {number} [options.minContentLength=140] The minimum node content length used to decide if the document is readerable.
 * @param {number} [options.minScore=20] The minumum cumulated 'score' used to determine if the document is readerable.
 * @param {Function} [options.visibilityChecker=isNodeVisible] The function used to determine if a node is visible.
 * @return {boolean} Whether or not we suspect Readability.parse() will suceeed at returning an article object.
 */
export function isProbablyReaderable(doc: any, options?: {
    minContentLength?: number;
    minScore?: number;
    visibilityChecker?: Function;
}): boolean;
