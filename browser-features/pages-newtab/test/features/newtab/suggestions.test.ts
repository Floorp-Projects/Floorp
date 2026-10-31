// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { selectHistorySuggestions } from "../../../src/features/newtab/suggestions.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "History suggestions exclude saved sites, sponsors and hidden URLs",
    fn: () => {
      const sites = ["saved", "sponsor", "hidden", "new"].map((name) => ({
        title: name,
        url: `https://${name}.test/#section`,
      }));
      const result = selectHistorySuggestions(
        sites,
        [{
          guid: "saved_______",
          parentGuid: "group_______",
          type: "bookmark",
          title: "Saved",
          url: "https://saved.test/",
          index: 0,
        }],
        [{ id: "placement", title: "Sponsor", url: "https://sponsor.test/" }],
        ["https://hidden.test/"],
      );
      assertEquals(
        result.map((site) => site.title).join(","),
        "new",
        "Do not repeat existing tiles or hidden suggestions",
      );
    },
  },
  {
    name: "History ordering and query identity survive deduplication",
    fn: () => {
      const sites = [
        "https://site.test?q=1",
        "https://site.test/?q=1#part",
        "https://site.test/?q=2",
      ]
        .map((url, index) => ({ url, title: String(index) }));
      const result = selectHistorySuggestions(sites, [], [], []);
      assertEquals(
        result.map((site) => site.title).join(","),
        "0,2",
        "Ignore fragments without merging distinct query URLs",
      );
      assertEquals(
        sites.length,
        3,
        "The source API response remains unchanged",
      );
    },
  },
  {
    name: "Unsafe history URLs cannot become clickable suggestions",
    fn: () => {
      const sites = [
        "javascript:alert(1)",
        "file:///private",
        "https://user:secret@site.test/",
        "invalid",
        "https://safe.test/",
      ]
        .map((url) => ({ url, title: url }));
      assertEquals(
        selectHistorySuggestions(sites, [], [], []).map((site) => site.url).join(","),
        "https://safe.test/",
        "Accept only credential-free HTTP(S)",
      );
    },
  },
];
export async function runAllTests(): Promise<void> {
  await runTests("suggestions.test.ts", tests);
}
