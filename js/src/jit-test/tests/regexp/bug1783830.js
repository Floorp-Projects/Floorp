assertEq(/[^!]/u.exec('\u{1F4A9}')[0], "\u{1F4A9}");
assertEq(/^[^!]/u.exec('\u{1F4A9}')[0], "\u{1F4A9}");
assertEq(/[^!]$/u.exec('\u{1F4A9}')[0], "\u{1F4A9}");
assertEq(/![^!]/u.exec('!\u{1F4A9}')[0], "!\u{1F4A9}");
assertEq(/[^!]!/u.exec('\u{1F4A9}!')[0], "\u{1F4A9}!");
assertEq(/![^!]/ui.exec('!\u{1F4A9}')[0], "!\u{1F4A9}");
assertEq(/[^!]!/ui.exec('\u{1F4A9}!')[0], "\u{1F4A9}!");
