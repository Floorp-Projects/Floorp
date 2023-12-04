import { ICU4XIsoTimeZoneFormat } from "./ICU4XIsoTimeZoneFormat";
import { ICU4XIsoTimeZoneMinuteDisplay } from "./ICU4XIsoTimeZoneMinuteDisplay";
import { ICU4XIsoTimeZoneSecondDisplay } from "./ICU4XIsoTimeZoneSecondDisplay";

/**
 */
export class ICU4XIsoTimeZoneOptions {
  format: ICU4XIsoTimeZoneFormat;
  minutes: ICU4XIsoTimeZoneMinuteDisplay;
  seconds: ICU4XIsoTimeZoneSecondDisplay;
}
