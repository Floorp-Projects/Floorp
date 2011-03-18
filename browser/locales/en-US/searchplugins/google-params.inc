  <Param name="q" value="{searchTerms}"/>
  <Param name="ie" value="utf-8"/>
  <Param name="oe" value="utf-8"/>
  <Param name="aq" value="t"/>
  <!-- Dynamic parameters -->
  <Param name="rls" value="{moz:distributionID}:{moz:locale}:{moz:official}"/>
#if MOZ_UPDATE_CHANNEL == beta
  <MozParam name="client" condition="defaultEngine" trueValue="firefox-beta" falseValue="firefox"/>
#elif MOZ_UPDATE_CHANNEL == aurora
  <MozParam name="client" condition="defaultEngine" trueValue="firefox-aurora" falseValue="firefox"/>
#elif MOZ_UPDATE_CHANNEL == nightly
  <MozParam name="client" condition="defaultEngine" trueValue="firefox-nightly" falseValue="firefox"/>
#else
  <MozParam name="client" condition="defaultEngine" trueValue="firefox-a" falseValue="firefox"/>
#endif
