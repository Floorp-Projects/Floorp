// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#include "../../include/ICU4XCollator.hpp"
#include "../../include/ICU4XDataProvider.hpp"
#include "../../include/ICU4XLocale.hpp"
#include "../../include/ICU4XLogger.hpp"
#include "../../include/ICU4XOrdering.hpp"

#include <iostream>
#include <string_view>

int main() {
  ICU4XLogger::init_simple_logger();
  ICU4XDataProvider dp = ICU4XDataProvider::create_compiled();

  // test 01 - basic collation example, default CollatorOptions

  std::string_view manna{ "manna" };
  std::string_view manana{ "mañana" };

  ICU4XLocale locale = ICU4XLocale::create_from_string("en").ok().value();
  ICU4XCollatorOptionsV1 options = {};
  ICU4XCollator collator = ICU4XCollator::create_v1(dp, locale, options).ok().value();
  ICU4XOrdering actual = collator.compare(manna, manana);

  if (actual != ICU4XOrdering::Greater) {
    std::cout << "Expected manna > mañana for locale " << locale.to_string().ok().value() << std::endl;
    return 1;
  }

  locale = ICU4XLocale::create_from_string("es").ok().value();
  collator = ICU4XCollator::create_v1(dp, locale, options).ok().value();
  actual = collator.compare(manna, manana);

  if (actual != ICU4XOrdering::Less) {
    std::cout << "Expected manna < mañana for locale " << locale.to_string().ok().value()<< std::endl;
    return 1;
  }

  // test 02 - collation strength example, requires non-default CollatorOptions

  std::string_view as{ "as" };
  std::string_view graveAs{ "às" };

  locale = ICU4XLocale::create_from_string("en").ok().value();
  options.strength = ICU4XCollatorStrength::Primary;
  collator = ICU4XCollator::create_v1(dp, locale, options).ok().value();
  actual = collator.compare(as, graveAs);

  if (actual != ICU4XOrdering::Equal) {
    std::cout << "Expected as = às for primary strength, locale " << locale.to_string().ok().value()<< std::endl;
    return 1;
  }

  options.strength = ICU4XCollatorStrength::Secondary;
  collator = ICU4XCollator::create_v1(dp, locale, options).ok().value();
  actual = collator.compare(as, graveAs);

  if (actual != ICU4XOrdering::Less) {
    std::cout << "Expected as < às for secondary strength, locale " << locale.to_string().ok().value()<< std::endl;
    return 1;
  }

  return 0;
}
