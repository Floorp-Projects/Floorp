import { ICU4XDataProvider, ICU4XDateLength, ICU4XDateTime, ICU4XDateTimeFormatter, ICU4XLocale, ICU4XTimeLength, ICU4XCalendar } from "icu4x";
import { Ok, Result, result, unwrap } from "./index";

export class DateTimeDemo {
    #displayFn: (formatted: string) => void;
    #dataProvider: ICU4XDataProvider;

    #localeStr: string;
    #calendarStr: string;
    #dateTimeStr: string;
    #locale: Result<ICU4XLocale>;
    #calendar: Result<ICU4XCalendar>;
    #dateLength: ICU4XDateLength;
    #timeLength: ICU4XTimeLength;

    #formatter: Result<ICU4XDateTimeFormatter>;
    #dateTime: Result<ICU4XDateTime> | null;

    constructor(displayFn: (formatted: string) => void, dataProvider: ICU4XDataProvider) {
        this.#displayFn = displayFn;
        this.#dataProvider = dataProvider;

        this.#locale = Ok(ICU4XLocale.create_from_string("en"));
        this.#calendar = Ok(ICU4XCalendar.create_for_locale(dataProvider, unwrap(this.#locale)));
        this.#dateLength = ICU4XDateLength.Short;
        this.#timeLength = ICU4XTimeLength.Short;
        this.#dateTime = null;
        this.#dateTimeStr = "";
        this.#calendarStr = "from-locale";
        this.#localeStr = "en";
        this.#updateFormatter();
    }

    setCalendar(calendar: string): void {
        this.#calendarStr = calendar;
        this.#updateLocaleAndCalendar();
        this.#updateFormatter();
    }

    setLocale(locid: string): void {
        this.#localeStr = locid;
        this.#updateLocaleAndCalendar();
        this.#updateFormatter();
    }

    #updateLocaleAndCalendar(): void {
        let locid = this.#localeStr;
        if (this.#calendarStr != "from-locale") {
            if (locid.indexOf("-u-") == -1) {
                locid = `${locid}-u-ca-${this.#calendarStr}`;
            } else {
                // Don't bother trying to patch up the situation where a calendar
                // is already specified; this is GIGO and the current locale parsing behavior
                // will just default to the first one (#calendarStr)
                locid = locid.replace("-u-", `-u-ca-${this.#calendarStr}-`);
            }
        }
        this.#locale = result(() => ICU4XLocale.create_from_string(locid));
        this.#calendar = result(() => ICU4XCalendar.create_for_locale(this.#dataProvider, unwrap(this.#locale) ));
        this.#updateDateTime();
    }

    setDateLength(dateLength: string): void {
        this.#dateLength = ICU4XDateLength[dateLength];
        this.#updateFormatter()
    }

    setTimeLength(timeLength: string): void {
        this.#timeLength = ICU4XTimeLength[timeLength];
        this.#updateFormatter()
    }

    setDateTime(dateTime: string): void {
        this.#dateTimeStr = dateTime;
        this.#updateDateTime();
        this.#render()
    }

    #updateDateTime(): void {
        const date = new Date(this.#dateTimeStr);

        this.#dateTime = result(() => ICU4XDateTime.create_from_iso_in_calendar(
            date.getFullYear(),
            date.getMonth() + 1,
            date.getDate(),
            date.getHours(),
            date.getMinutes(),
            date.getSeconds(),
            0,
            unwrap(this.#calendar)
        ));
    }

    #updateFormatter(): void {
        this.#formatter = result(() => ICU4XDateTimeFormatter.create_with_lengths(
            this.#dataProvider,
            unwrap(this.#locale),
            this.#dateLength,
            this.#timeLength,
        ));
        this.#render();
    }

    #render(): void {
        try {
            const formatter = unwrap(this.#formatter);
            if (this.#dateTime !== null) {
                const dateTime = unwrap(this.#dateTime);
                this.#displayFn(formatter.format_datetime(dateTime));
            } else {
                this.#displayFn("");
            }
        } catch (e) {
            if (e.error_value) {
                this.#displayFn(`ICU4X Error: ${e.error_value}`);
            } else {
                this.#displayFn(`Unexpected Error: ${e}`);
            }
        }
    }
}

export function setup(dataProvider: ICU4XDataProvider): void {
    const formattedDateTime = document.getElementById('dtf-formatted') as HTMLInputElement;
    const dateTimeDemo = new DateTimeDemo((formatted) => formattedDateTime.innerText = formatted, dataProvider);

    const otherLocaleBtn = document.getElementById('dtf-locale-other') as HTMLInputElement | null;
    otherLocaleBtn?.addEventListener('click', () => {
        dateTimeDemo.setLocale(otherLocaleInput.value);
    });

    const otherLocaleInput = document.getElementById('dtf-locale-other-input') as HTMLInputElement | null;
    otherLocaleInput?.addEventListener('input', () => {
        const otherLocaleBtn = document.getElementById('dtf-locale-other') as HTMLInputElement | null;
        if (otherLocaleBtn != null) {
            otherLocaleBtn.checked = true;
            dateTimeDemo.setLocale(otherLocaleInput.value);
        }
    });

    for (let input of document.querySelectorAll<HTMLInputElement | null>('input[name="dtf-locale"]')) {
        if (input?.value !== 'other') {
            input.addEventListener('input', () => {
                dateTimeDemo.setLocale(input.value)
            });
        }
    }
    for (let selector of document.querySelectorAll<HTMLSelectElement | null>('select[name="dtf-calendar"]')) {
        // <select> doesn't have oninput
        selector?.addEventListener('change', () => {
            dateTimeDemo.setCalendar(selector.value)
        });
    }

    for (let input of document.querySelectorAll<HTMLInputElement | null>('input[name="dtf-date-length"]')) {
        input?.addEventListener('input', () => {
            dateTimeDemo.setDateLength(input.value)
        });
    }

    for (let input of document.querySelectorAll<HTMLInputElement | null>('input[name="dtf-time-length"]')) {
        input?.addEventListener('input', () => {
            dateTimeDemo.setTimeLength(input.value)
        });
    }

    const inputDateTime = document.getElementById('dtf-input') as HTMLInputElement | null;
    inputDateTime?.addEventListener('input', () => {
        dateTimeDemo.setDateTime(inputDateTime.value)
    });
    
    // Annoyingly `toISOString()` gets us the format we need, but it converts to UTC first
    // We instead get the current datetime and recast it to a date that is the current datetime
    // when represented in UTC
    let now = new Date();
    const offset = now.getTimezoneOffset();
    now.setMinutes(now.getMinutes() - offset);
    const nowISO = now.toISOString().slice(0,16);
    if (inputDateTime != undefined) {
        // this seems like the best way to get something compatible with inputDateTIme
        inputDateTime.value = nowISO;
    }
    dateTimeDemo.setDateTime(nowISO);
}
