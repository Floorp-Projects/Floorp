// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let scrambled = ['𠙶', '𠇲', '㓙', '㑧', '假', '凷'];

// Root or pinyin
const fallback = ["假", "凷", "𠙶", "㑧", "㓙", "𠇲"];

scrambled.sort(new Intl.Collator("zh-u-co-big5han").compare);
assertEqArray(scrambled, fallback);

scrambled.sort(new Intl.Collator("zh-u-co-gb2312").compare);
assertEqArray(scrambled, fallback);

assertEq(new Intl.Collator("zh-u-co-big5han").resolvedOptions().collation, "default");
assertEq(new Intl.Collator("zh-u-co-gb2312").resolvedOptions().collation, "default");

assertEq(Intl.supportedValuesOf("collation").includes("big5han"), false);
assertEq(Intl.supportedValuesOf("collation").includes("gb2312"), false);

reportCompare(0, 0, 'ok');
