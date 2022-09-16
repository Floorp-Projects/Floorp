/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/NullPrincipal.h"
#include "nsIURIMutator.h"
#include "nsPrintfCString.h"

namespace mozilla {

TEST(NullPrincipalPrecursor, EscapingRoundTrips)
{
  nsTArray<nsCString> inputs;

  inputs.AppendElements(mozilla::Span(std::array{
      "mailbox:///dev/shm/tmp5wkt9ff_.mozrunner/Mail/Local%20Folders/secure-mail?number=5"_ns,
  }));

  // Add a string for every ASCII byte both escaped and unescaped.
  for (uint8_t c = 0; c < 128; ++c) {
    inputs.AppendElement(nsPrintfCString("%02X: %c", c, (char)c));
    inputs.AppendElement(nsPrintfCString("%02X: %%%02X", c, c));
  }

  nsID dummyID{0xddf15eaf,
               0x3837,
               0x4678,
               {0x80, 0x3b, 0x86, 0x86, 0xe8, 0x17, 0x66, 0x71}};
  nsCOMPtr<nsIURI> baseURI = NullPrincipal::CreateURI(nullptr, &dummyID);
  ASSERT_TRUE(baseURI);

  for (auto& input : inputs) {
    // First build an escaped version of the input string using
    // `EscapePrecursorQuery`.
    nsCString escaped(input);
    NullPrincipal::EscapePrecursorQuery(escaped);

    // Make sure that this escaped URI round-trips through a `moz-nullprincipal`
    // URI's query without any additional escapes.
    nsCOMPtr<nsIURI> clone;
    EXPECT_NS_SUCCEEDED(
        NS_MutateURI(baseURI).SetQuery(escaped).Finalize(clone));
    nsCString query;
    EXPECT_NS_SUCCEEDED(clone->GetQuery(query));
    EXPECT_EQ(escaped, query);

    // Try to unescape our escaped URI and make sure we recover the input
    // string.
    nsCString unescaped(escaped);
    NullPrincipal::UnescapePrecursorQuery(unescaped);
    EXPECT_EQ(input, unescaped);
  }
}

}  // namespace mozilla
