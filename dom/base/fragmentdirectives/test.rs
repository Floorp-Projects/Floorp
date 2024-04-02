/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(test)]
mod test {
    use crate::fragment_directive_impl::{
        create_fragment_directive_string, parse_fragment_directive_and_remove_it_from_hash,
        TextDirective,
    };

    /// This test verifies that valid combinations of [prefix-,]start[,end][,-suffix] are parsed correctly.
    #[test]
    fn test_parse_fragment_directive_with_one_text_directive() {
        let test_cases = vec![
            ("#:~:text=start", (None, Some("start"), None, None)),
            (
                "#:~:text=start,end",
                (None, Some("start"), Some("end"), None),
            ),
            (
                "#:~:text=prefix-,start",
                (Some("prefix"), Some("start"), None, None),
            ),
            (
                "#:~:text=prefix-,start,end",
                (Some("prefix"), Some("start"), Some("end"), None),
            ),
            (
                "#:~:text=prefix-,start,end,-suffix",
                (Some("prefix"), Some("start"), Some("end"), Some("suffix")),
            ),
            (
                "#:~:text=start,-suffix",
                (None, Some("start"), None, Some("suffix")),
            ),
            (
                "#:~:text=start,end,-suffix",
                (None, Some("start"), Some("end"), Some("suffix")),
            ),
            ("#:~:text=text=", (None, Some("text="), None, None)),
        ];
        for (url, (prefix, start, end, suffix)) in test_cases {
            let (stripped_url, fragment_directive, result) =
                parse_fragment_directive_and_remove_it_from_hash(&url)
                    .expect("The parser must find a result.");
            assert_eq!(
                fragment_directive,
                &url[4..],
                "The extracted fragment directive string
                should be unsanitized and therefore match the input string."
            );
            assert_eq!(result.len(), 1, "There must be one parsed text fragment.");
            assert_eq!(
                stripped_url, "",
                "The fragment directive must be removed from the url hash."
            );
            let text_directive = result.first().unwrap();
            if prefix.is_none() {
                assert!(
                    text_directive.prefix().is_none(),
                    "There must be no `prefix` token (test case `{}`).",
                    url
                );
            } else {
                assert!(
                    text_directive
                        .prefix()
                        .as_ref()
                        .expect("There must be a `prefix` token.")
                        .value()
                        == prefix.unwrap(),
                    "Wrong value for `prefix` (test case `{}`).",
                    url
                );
            }
            if start.is_none() {
                assert!(
                    text_directive.start().is_none(),
                    "There must be no `start` token (test case `{}`).",
                    url
                );
            } else {
                assert!(
                    text_directive
                        .start()
                        .as_ref()
                        .expect("There must be a `start` token.")
                        .value()
                        == start.unwrap(),
                    "Wrong value for `start` (test case `{}`).",
                    url
                );
            }
            if end.is_none() {
                assert!(
                    text_directive.end().is_none(),
                    "There must be no `end` token (test case `{}`).",
                    url
                );
            } else {
                assert!(
                    text_directive
                        .end()
                        .as_ref()
                        .expect("There must be a `end` token.")
                        .value()
                        == end.unwrap(),
                    "Wrong value for `end` (test case `{}`).",
                    url
                );
            }
            if suffix.is_none() {
                assert!(
                    text_directive.suffix().is_none(),
                    "There must be no `suffix` token (test case `{}`).",
                    url
                );
            } else {
                assert!(
                    text_directive
                        .suffix()
                        .as_ref()
                        .expect("There must be a `suffix` token.")
                        .value()
                        == suffix.unwrap(),
                    "Wrong value for `suffix` (test case `{}`).",
                    url
                );
            }
        }
    }

    #[test]
    fn test_parse_full_url() {
        for (url, stripped_url_ref) in [
            ("https://example.com#:~:text=foo", "https://example.com"),
            (
                "https://example.com/some/page.html?query=answer#:~:text=foo",
                "https://example.com/some/page.html?query=answer",
            ),
            (
                "https://example.com/some/page.html?query=answer#fragment:~:text=foo",
                "https://example.com/some/page.html?query=answer#fragment",
            ),
            (
                "http://example.com/page.html?query=irrelevant:~:#bar:~:text=foo",
                "http://example.com/page.html?query=irrelevant:~:#bar"
            )
        ] {
            let (stripped_url, fragment_directive, _) =
                parse_fragment_directive_and_remove_it_from_hash(&url)
                    .expect("The parser must find a result");
            assert_eq!(stripped_url, stripped_url_ref, "The stripped url is not correct.");
            assert_eq!(fragment_directive, "text=foo");
        }
    }

    /// This test verifies that a text fragment is parsed correctly if it is preceded
    /// or followed by a fragment (i.e. `#foo:~:text=bar`).
    #[test]
    fn test_parse_text_fragment_after_fragments() {
        let url = "#foo:~:text=start";
        let (stripped_url, fragment_directive, result) =
            parse_fragment_directive_and_remove_it_from_hash(&url)
                .expect("The parser must find a result.");
        assert_eq!(
            result.len(),
            1,
            "There must be exactly one parsed text fragment."
        );
        assert_eq!(
            stripped_url, "#foo",
            "The fragment directive was not removed correctly."
        );
        assert_eq!(
            fragment_directive, "text=start",
            "The fragment directive was not extracted correctly."
        );
        let fragment = result.first().unwrap();
        assert!(fragment.prefix().is_none(), "There is no `prefix` token.");
        assert_eq!(
            fragment
                .start()
                .as_ref()
                .expect("There must be a `start` token.")
                .value(),
            "start"
        );
        assert!(fragment.end().is_none(), "There is no `end` token.");
        assert!(fragment.suffix().is_none(), "There is no `suffix` token.");
    }

    /// Ensure that multiple text fragments are parsed correctly.
    #[test]
    fn test_parse_multiple_text_fragments() {
        let url = "#:~:text=prefix-,start,-suffix&text=foo&text=bar,-suffix";
        let (_, _, text_directives) =
            parse_fragment_directive_and_remove_it_from_hash(&url)
                .expect("The parser must find a result.");
        assert_eq!(
            text_directives.len(),
            3,
            "There must be exactly two parsed text fragments."
        );
        let first_text_directive = &text_directives[0];
        assert_eq!(
            first_text_directive
                .prefix()
                .as_ref()
                .expect("There must be a `prefix` token.")
                .value(),
            "prefix"
        );
        assert_eq!(
            first_text_directive
                .start()
                .as_ref()
                .expect("There must be a `start` token.")
                .value(),
            "start"
        );
        assert!(
            first_text_directive.end().is_none(),
            "There is no `end` token."
        );
        assert_eq!(
            first_text_directive
                .suffix()
                .as_ref()
                .expect("There must be a `suffix` token.")
                .value(),
            "suffix"
        );

        let second_text_directive = &text_directives[1];
        assert!(
            second_text_directive.prefix().is_none(),
            "There is no `prefix` token."
        );
        assert_eq!(
            second_text_directive
                .start()
                .as_ref()
                .expect("There must be a `start` token.")
                .value(),
            "foo"
        );
        assert!(
            second_text_directive.end().is_none(),
            "There is no `end` token."
        );
        assert!(
            second_text_directive.suffix().is_none(),
            "There is no `suffix` token."
        );
        let third_text_directive = &text_directives[2];
        assert!(
            third_text_directive.prefix().is_none(),
            "There is no `prefix` token."
        );
        assert_eq!(
            third_text_directive
                .start()
                .as_ref()
                .expect("There must be a `start` token.")
                .value(),
            "bar"
        );
        assert!(
            third_text_directive.end().is_none(),
            "There is no `end` token."
        );
        assert_eq!(
            third_text_directive
                .suffix()
                .as_ref()
                .expect("There must be a `suffix` token.")
                .value(),
            "suffix"
        );
    }

    /// Multiple text directives should be parsed correctly
    /// if they are surrounded or separated by unknown directives.
    #[test]
    fn test_parse_multiple_text_directives_with_unknown_directive_in_between() {
        for url in [
            "#:~:foo&text=start1&text=start2",
            "#:~:text=start1&foo&text=start2",
            "#:~:text=start1&text=start2&foo",
        ] {
            let (_, fragment_directive, text_directives) =
                parse_fragment_directive_and_remove_it_from_hash(&url)
                    .expect("The parser must find a result.");
            assert_eq!(
                fragment_directive,
                &url[4..],
                "The extracted fragment directive string is unsanitized
                and should contain the unknown directive."
            );
            assert_eq!(
                text_directives.len(),
                2,
                "There must be exactly two parsed text fragments."
            );
            let first_text_directive = &text_directives[0];
            assert_eq!(
                first_text_directive
                    .start()
                    .as_ref()
                    .expect("There must be a `start` token.")
                    .value(),
                "start1"
            );
            let second_text_directive = &text_directives[1];
            assert_eq!(
                second_text_directive
                    .start()
                    .as_ref()
                    .expect("There must be a `start` token.")
                    .value(),
                "start2"
            );
        }
    }

    /// Ensures that input that doesn't contain a text fragment does not produce a result.
    /// This includes the use of partial identifying tokens necessary for a text fragment
    /// (e.g. `:~:` without `text=`, `text=foo` without the `:~:` or multiple occurrences of `:~:`)
    /// In these cases, the parser must return `None` to indicate that there are no valid text fragments.
    #[test]
    fn test_parse_invalid_or_unknown_fragment_directive() {
        for url in [
            "#foo",
            "#foo:",
            "#foo:~:",
            "#foo:~:bar",
            "text=prefix-,start",
            "#:~:text=foo-,bar,-baz:~:text=foo",
        ] {
            let text_directives =
                parse_fragment_directive_and_remove_it_from_hash(&url);
            assert!(
                text_directives.is_none(),
                "The fragment `{}` does not contain a valid or known fragment directive.",
                url
            );
        }
    }

    /// Ensures that ill-formed text directives (but valid fragment directives)
    /// (starting correctly with `:~:text=`) are not parsed.
    /// Instead `None` must be returned.
    /// Test cases include invalid combinations of `prefix`/`suffix`es,
    /// additional `,`s, too many `start`/`end` tokens, or empty text fragments.
    #[test]
    fn test_parse_invalid_text_fragments() {
        for url in [
            "#:~:text=start,start,start",
            "#:~:text=prefix-,prefix-",
            "#:~:text=prefix-,-suffix",
            "#:~:text=prefix-,start,start,start",
            "#:~:text=prefix-,start,start,start,-suffix",
            "#:~:text=start,start,start,-suffix",
            "#:~:text=prefix-,start,end,-suffix,foo",
            "#:~:text=foo,prefix-,start",
            "#:~:text=prefix-,,start,",
            "#:~:text=,prefix,start",
            "#:~:text=",
        ] {
            let text_directives =
                parse_fragment_directive_and_remove_it_from_hash(&url);
            assert!(
                text_directives.is_none(),
                "The fragment directive `{}` does not contain a valid text directive.",
                url
            );
        }
    }

    /// Ensure that out of multiple text fragments only the invalid ones are ignored
    /// while valid text fragments are still returned.
    /// Since correct parsing of multiple text fragments as well as
    /// several forms of invalid text fragments are already tested in
    /// `test_parse_multiple_text_fragments` and `test_parse_invalid_text_fragments()`,
    /// it should be enough to test this with only one fragment directive
    /// that contains two text fragments, one of them being invalid.
    #[test]
    fn test_valid_and_invalid_text_directives() {
        for url in [
            "#:~:text=start&text=,foo,",
            "#:~:text=foo,foo,foo&text=start",
        ] {
            let (_, fragment_directive, text_directives) =
                parse_fragment_directive_and_remove_it_from_hash(&url)
                    .expect("The parser must find a result.");
            assert_eq!(
                fragment_directive,
                &url[4..],
                "The extracted fragment directive string is unsanitized
                and should contain invalid text directives."
            );
            assert_eq!(
                text_directives.len(),
                1,
                "There must be exactly one parsed text fragment."
            );
            let text_directive = text_directives.first().unwrap();
            assert_eq!(
                text_directive
                    .start()
                    .as_ref()
                    .expect("There must be a `start` value.")
                    .value(),
                "start",
                "The `start` value of the text directive has the wrong value."
            );
        }
    }

    /// Ensures that a fragment directive that contains percent-encoded characters
    /// is decoded correctly. This explicitly includes characters which are used
    /// for identifying text fragments, i.e. `#`, `, `, `&`, `:`, `~` and `-`.
    #[test]
    fn test_parse_percent_encoding_tokens() {
        let url = "#:~:text=prefix%26-,start%20and%2C,end%23,-%26suffix%2D";
        let (_, fragment_directive, text_directives) =
            parse_fragment_directive_and_remove_it_from_hash(&url)
                .expect("The parser must find a result.");
        assert_eq!(
            fragment_directive,
            &url[4..],
            "The extracted fragment directive string is unsanitized
                and should contain the original and percent-decoded string."
        );
        let text_directive = text_directives.first().unwrap();
        assert_eq!(
            text_directive
                .prefix()
                .as_ref()
                .expect("There must be a prefix.")
                .value(),
            "prefix&",
            ""
        );
        assert_eq!(
            text_directive
                .start()
                .as_ref()
                .expect("There must be a prefix.")
                .value(),
            "start and,",
            ""
        );
        assert_eq!(
            text_directive
                .end()
                .as_ref()
                .expect("There must be a prefix.")
                .value(),
            "end#",
            ""
        );
        assert_eq!(
            text_directive
                .suffix()
                .as_ref()
                .expect("There must be a prefix.")
                .value(),
            "&suffix-",
            ""
        );
    }

    /// Ensures that a text fragment is created correctly,
    /// based on a given combination of tokens.
    /// This includes all sorts of combinations of
    /// `prefix`, `suffix`, `start` and `end`,
    /// als well as values for these tokens which contain
    /// characters that need to be encoded because they are
    /// identifiers for text fragments
    /// (#`, `, `, `&`, `:`, `~` and `-`).
    #[test]
    fn test_create_fragment_directive() {
        for (text_directive, expected_fragment_directive) in [
            (
                TextDirective::from_parts(
                    String::new(),
                    String::from("start"),
                    String::new(),
                    String::new(),
                )
                .unwrap(),
                ":~:text=start",
            ),
            (
                TextDirective::from_parts(
                    String::new(),
                    String::from("start"),
                    String::from("end"),
                    String::new(),
                )
                .unwrap(),
                ":~:text=start,end",
            ),
            (
                TextDirective::from_parts(
                    String::from("prefix"),
                    String::from("start"),
                    String::from("end"),
                    String::new(),
                )
                .unwrap(),
                ":~:text=prefix-,start,end",
            ),
            (
                TextDirective::from_parts(
                    String::from("prefix"),
                    String::from("start"),
                    String::from("end"),
                    String::from("suffix"),
                )
                .unwrap(),
                ":~:text=prefix-,start,end,-suffix",
            ),
            (
                TextDirective::from_parts(
                    String::new(),
                    String::from("start"),
                    String::from("end"),
                    String::from("suffix"),
                )
                .unwrap(),
                ":~:text=start,end,-suffix",
            ),
            (
                TextDirective::from_parts(
                    String::from("prefix"),
                    String::from("start"),
                    String::new(),
                    String::from("suffix"),
                )
                .unwrap(),
                ":~:text=prefix-,start,-suffix",
            ),
            (
                TextDirective::from_parts(
                    String::from("prefix-"),
                    String::from("start and,"),
                    String::from("&end"),
                    String::from("#:~:suffix"),
                )
                .unwrap(),
                ":~:text=prefix%2D-,start%20and%2C,%26end,-%23%3A%7E%3Asuffix",
            ),
        ] {
            let fragment_directive = create_fragment_directive_string(&vec![text_directive])
                .expect("The given input must produce a valid fragment directive.");
            assert_eq!(fragment_directive, expected_fragment_directive);
        }
    }

    /// Ensures that a fragment directive is created correctly if multiple text fragments are given.
    /// The resulting fragment must start with `:~:`
    /// and each text fragment must be separated using `&text=`.
    #[test]
    fn test_create_fragment_directive_from_multiple_text_directives() {
        let text_directives = vec![
            TextDirective::from_parts(
                String::new(),
                String::from("start1"),
                String::new(),
                String::new(),
            )
            .unwrap(),
            TextDirective::from_parts(
                String::new(),
                String::from("start2"),
                String::new(),
                String::new(),
            )
            .unwrap(),
            TextDirective::from_parts(
                String::new(),
                String::from("start3"),
                String::new(),
                String::new(),
            )
            .unwrap(),
        ];
        let fragment_directive = create_fragment_directive_string(&text_directives)
            .expect("The given input must produce a valid fragment directive.");
        assert_eq!(
            fragment_directive, ":~:text=start1&text=start2&text=start3",
            "The created fragment directive is wrong for multiple fragments."
        );
    }
}
