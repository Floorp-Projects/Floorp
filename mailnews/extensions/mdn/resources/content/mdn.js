/*
 * default prefs for mdn
 */

pref("mail.identity.default.use_custom_prefs", false);            // false: Use global true: Use custom

pref("mail.identity.default.request_return_receipt_on", false);

pref("mail.server.default.incorporate_return_receipt", 0);       // 0: Inbox/filter 1: Sent folder

pref("mail.server.default.mdn_report_enabled", true);            // false: Never return receipts true: Return some receipts

pref("mail.server.default.mdn_not_in_to_cc", 2);                 // 0: Never 1: Always 2: Ask me 3: Denial
pref("mail.server.default.mdn_outside_domain", 2); 
pref("mail.server.default.mdn_other", 2); 

pref("mail.identity.default.request_receipt_header_type", 0); // return receipt header type - 0: MDN-DNT 1: RRT 2: Both

pref("mail.server.default.mdn_report_enabled", true);
