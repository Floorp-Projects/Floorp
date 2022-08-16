/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::xpcom_utils::get_app_locales;
use cstr::cstr;
use fluent_fallback::env::LocalesProvider;
use l10nregistry::{
    env::ErrorReporter,
    errors::{L10nRegistryError, L10nRegistrySetupError},
};
use log::warn;
use nserror::{nsresult, NS_ERROR_NOT_AVAILABLE};
use nsstring::{nsCStr, nsString};
use std::fmt::{self, Write};
use unic_langid::LanguageIdentifier;
use xpcom::interfaces;

#[derive(Clone)]
pub struct GeckoEnvironment {
    custom_locales: Option<Vec<LanguageIdentifier>>,
}

impl GeckoEnvironment {
    pub fn new(custom_locales: Option<Vec<LanguageIdentifier>>) -> Self {
        Self { custom_locales }
    }

    pub fn report_l10nregistry_setup_error(error: &L10nRegistrySetupError) {
        warn!("L10nRegistry setup error: {}", error);
        let result = log_simple_console_error(
            &error.to_string(),
            false,
            true,
            None,
            (0, 0),
            interfaces::nsIScriptError::errorFlag as u32,
        );
        if let Err(err) = result {
            warn!("Error while reporting an error: {}", err);
        }
    }
}

impl ErrorReporter for GeckoEnvironment {
    fn report_errors(&self, errors: Vec<L10nRegistryError>) {
        for error in errors {
            warn!("L10nRegistry error: {}", error);
            let result = match error {
                L10nRegistryError::FluentError {
                    resource_id,
                    loc,
                    error,
                } => log_simple_console_error(
                    &error.to_string(),
                    false,
                    true,
                    Some(nsString::from(&resource_id.value)),
                    loc.map_or((0, 0), |(l, c)| (l as u32, c as u32)),
                    interfaces::nsIScriptError::errorFlag as u32,
                ),
                L10nRegistryError::MissingResource { .. } => log_simple_console_error(
                    &error.to_string(),
                    false,
                    true,
                    None,
                    (0, 0),
                    interfaces::nsIScriptError::warningFlag as u32,
                ),
            };
            if let Err(err) = result {
                warn!("Error while reporting an error: {}", err);
            }
        }
    }
}

impl LocalesProvider for GeckoEnvironment {
    type Iter = std::vec::IntoIter<unic_langid::LanguageIdentifier>;
    fn locales(&self) -> Self::Iter {
        if let Some(custom_locales) = &self.custom_locales {
            custom_locales.clone().into_iter()
        } else {
            let result = get_app_locales()
                .expect("Failed to retrieve app locales")
                .into_iter()
                .map(|s| LanguageIdentifier::from_bytes(&s).expect("Failed to parse a locale"))
                .collect::<Vec<_>>();
            result.into_iter()
        }
    }
}

fn log_simple_console_error(
    error: &impl fmt::Display,
    from_private_window: bool,
    from_chrome_context: bool,
    path: Option<nsString>,
    pos: (u32, u32),
    error_flags: u32,
) -> Result<(), nsresult> {
    // Format whatever error argument into a wide string with `Display`.
    let mut error_str = nsString::new();
    write!(&mut error_str, "{}", error).expect("nsString has an infallible Write impl");

    // Get the relevant services, and create the script error object.
    let console_service =
        xpcom::get_service::<interfaces::nsIConsoleService>(cstr!("@mozilla.org/consoleservice;1"))
            .ok_or(NS_ERROR_NOT_AVAILABLE)?;
    let script_error =
        xpcom::create_instance::<interfaces::nsIScriptError>(cstr!("@mozilla.org/scripterror;1"))
            .ok_or(NS_ERROR_NOT_AVAILABLE)?;
    let category = nsCStr::from("l10n");
    unsafe {
        script_error
            .Init(
                &*error_str,
                &*path.unwrap_or(nsString::new()), /* aSourceName */
                &*nsString::new(),                 /* aSourceLine */
                pos.0,                             /* aLineNumber */
                pos.1,                             /* aColNumber */
                error_flags,
                &*category,
                from_private_window,
                from_chrome_context,
            )
            .to_result()?;

        console_service.LogMessage(&**script_error).to_result()?;
    }
    Ok(())
}
