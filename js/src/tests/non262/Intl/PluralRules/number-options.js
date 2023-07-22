// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test number-format options are accepted by Intl.PluralRules.

for (let minimumIntegerDigits of [undefined, 1, 21]) {
  for (let minimumFractionDigits of [undefined, 0, 20]) {
    for (let maximumFractionDigits of [undefined, 0, 20]) {
      for (let minimumSignificantDigits of [undefined, 0, 21]) {
        for (let maximumSignificantDigits of [undefined, 0, 21]) {
          for (let roundingPriority of [undefined, "morePrecision"]) {
            for (let roundingIncrement of [undefined, 5000]) {
              for (let roundingMode of [undefined, "trunc"]) {
                for (let trailingZeroDisplay of [undefined, "stripIfInteger"]) {
                  let options = {
                    minimumIntegerDigits,
                    minimumFractionDigits,
                    maximumFractionDigits,
                    minimumSignificantDigits,
                    maximumSignificantDigits,
                    roundingPriority,
                    roundingIncrement,
                    roundingMode,
                    trailingZeroDisplay,
                  };

                  let pl;
                  try {
                    pl = new Intl.PluralRules("en", options);
                  } catch (e) {
                    // Ignore exception from conflicting options.
                    continue;
                  }

                  // InitializePluralRules:
                  // 8. Perform ? SetNumberFormatDigitOptions(pluralRules, options, +0𝔽, 3𝔽, "standard").
                  let mnfdDefault = 0;
                  let mxfdDefault = 3;

                  // SetNumberFormatDigitOptions:
                  // 13. If roundingIncrement is not 1, set mxfdDefault to mnfdDefault.
                  if (roundingIncrement > 1) {
                    mxfdDefault = mnfdDefault;
                  }

                  // 17. If mnsd is not undefined or mxsd is not undefined, then
                  //   a. Let hasSd be true.
                  // 18. Else,
                  //   a. Let hasSd be false.
                  let hasSd = minimumSignificantDigits !== undefined || maximumSignificantDigits !== undefined;

                  // 21. Let needSd be true.
                  // 22. Let needFd be true.
                  let needSd = true;
                  let needFd = true;

                  // 23. If roundingPriority is "auto", then
                  //   a. Set needSd to hasSd.
                  //   b. If needSd is true, or hasFd is false and notation is "compact", then
                  //     i. Set needFd to false.
                  if ((roundingPriority ?? "auto") === "auto") {
                    needSd = hasSd;
                    if (needSd) {
                      needFd = false;
                    }
                  }

                  // 24. If needSd is true, then
                  //   a. If hasSd is true, then
                  //     i. Set intlObj.[[MinimumSignificantDigits]] to ? DefaultNumberOption(mnsd, 1, 21, 1).
                  //     ii. Set intlObj.[[MaximumSignificantDigits]] to ? DefaultNumberOption(mxsd, intlObj.[[MinimumSignificantDigits]], 21, 21).
                  //   b. Else,
                  //     i. Set intlObj.[[MinimumSignificantDigits]] to 1.
                  //     ii. Set intlObj.[[MaximumSignificantDigits]] to 21.
                  let mnsd = undefined;
                  let mxsd = undefined;
                  if (needSd) {
                    mnsd = minimumSignificantDigits ?? 1;
                    mxsd = maximumSignificantDigits ?? 21;
                  }

                  // 25. If needFd is true, then
                  //   a. If hasFd is true, then
                  //     i. Set mnfd to ? DefaultNumberOption(mnfd, 0, 20, undefined).
                  //     ii. Set mxfd to ? DefaultNumberOption(mxfd, 0, 20, undefined).
                  //     iii. If mnfd is undefined, set mnfd to min(mnfdDefault, mxfd).
                  //     iv. Else if mxfd is undefined, set mxfd to max(mxfdDefault, mnfd).
                  //     v. Else if mnfd is greater than mxfd, throw a RangeError exception.
                  //     vi. Set intlObj.[[MinimumFractionDigits]] to mnfd.
                  //     vii. Set intlObj.[[MaximumFractionDigits]] to mxfd.
                  //   b. Else,
                  //     i. Set intlObj.[[MinimumFractionDigits]] to mnfdDefault.
                  //     ii. Set intlObj.[[MaximumFractionDigits]] to mxfdDefault.
                  let mnfd = undefined;
                  let mxfd = undefined;
                  if (needFd) {
                    mnfd = minimumFractionDigits ?? mnfdDefault;
                    mxfd = maximumFractionDigits ?? Math.max(mxfdDefault, mnfd);
                  }

                  // 26. If needSd is false and needFd is false, then
                  //   a. Set intlObj.[[MinimumFractionDigits]] to 0.
                  //   b. Set intlObj.[[MaximumFractionDigits]] to 0.
                  //   c. Set intlObj.[[MinimumSignificantDigits]] to 1.
                  //   d. Set intlObj.[[MaximumSignificantDigits]] to 2.
                  //   e. Set intlObj.[[RoundingType]] to morePrecision.
                  if (!needSd && !needFd) {
                    mnfd = 0;
                    mxfd = 0;
                    mnsd = 1;
                    mxsd = 2;
                  }

                  let resolved = pl.resolvedOptions();
                  assertEq(resolved.minimumIntegerDigits, minimumIntegerDigits ?? 1);
                  assertEq(resolved.minimumFractionDigits, mnfd);
                  assertEq(resolved.maximumFractionDigits, mxfd);
                  assertEq(resolved.minimumSignificantDigits, mnsd);
                  assertEq(resolved.maximumSignificantDigits, mxsd);
                  assertEq(resolved.roundingPriority, roundingPriority ?? "auto");
                  assertEq(resolved.roundingIncrement, roundingIncrement ?? 1);
                  assertEq(resolved.roundingMode, roundingMode ?? "halfExpand");
                  assertEq(resolved.trailingZeroDisplay, trailingZeroDisplay ?? "auto");
                }
              }
            }
          }
        }
      }
    }
  }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, 'ok');
