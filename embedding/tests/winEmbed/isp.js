//append mail and news accounts, establish internal server IDs
user_pref("mail.accountmanager.appendaccounts", "vendor_mail_account,vendor_news_account");
user_pref("mail.account.vendor_mail_account.identities", "vendor_mail");
user_pref("mail.account.vendor_news_account.identities", "vendor_news");


//server IDs
user_pref("mail.account.vendor_mail_account.server", "vendor_mail_server");
user_pref("mail.identity.vendor_mail.smtpServer", "vendor_smtp_server");
user_pref("mail.smtpservers.appendsmtpservers", "vendor_smtp_server");
user_pref("mail.smtp.defaultserver", "vendor_smtp_server");
user_pref("mail.account.vendor_news_account.server", "vendor_news_server");


//------------------------ server settigs ------------------------------

//mail
user_pref("mail.server.vendor_mail_server.name", "Vendor Mail");
user_pref("mail.server.vendor_mail_server.hostname", "vendor-mail-host-name");
user_pref("mail.server.vendor_mail_server.type", "imap");
user_pref("mail.smtpserver.vendor_smtp_server.hostname", "vendor-smtp-host-name");

//news
user_pref("mail.server.vendor_news_server.name", "Vendor News Account");
user_pref("mail.server.vendor_news_server.type", "nntp");
user_pref("mail.server.vendor_news_server.hostname", "vendor-news-host-name");

//------------------------ server settigs ------------------------------


//account setup wizard settings
user_pref("mail.identity.vendor_mail.smtpRequiresUsername", true);
user_pref("mail.identity.vendor_mail.wizardSkipPanels", true);
user_pref("mail.identity.vendor_mail.valid", false);


//news setup wizard settings
user_pref("mail.identity.vendor_news.valid", false);
user_pref("mail.identity.vendor_news.wizardSkipPanels", true);


//mail account functional settings
user_pref("mail.server.vendor_mail_server.login_at_startup", false);
user_pref("mail.server.vendor_mail_server.capability", 97087);
user_pref("mail.server.vendor_mail_server.max_cached_connections", 5);


//mail folder settings
user_pref("mail.identity.vendor_mail.drafts_folder_picker_mode", "0");
user_pref("mail.identity.vendor_mail.fcc_folder_picker_mode", "0");
user_pref("mail.identity.vendor_mail.tmpl_folder_picker_mode", "0");


//news account functional settings
user_pref("mail.server.vendor_news_server.login_at_startup", true);
user_pref("mail.identity.vendor_news.compose_html", false);


//configuration file. please replace both "netscp" and 
//"netscp.cfg" by "vendor" and "vendor.cfg", if needed
user_pref("general.config.vendor", "netscp"); 
user_pref("general.config.filename", "netscp.cfg"); 


//DO NOT MODIFY THIS SECTION
user_pref("mail.preconfigaccounts.added", false);
user_pref("mail.preconfigsmtpservers.added", false);
