[Constructor(DOMString type, optional PaymentMethodChangeEventInit eventInitDict),
 SecureContext,
 Exposed=Window,
 Func="mozilla::dom::PaymentRequest::PrefEnabled"]
interface PaymentMethodChangeEvent : PaymentRequestUpdateEvent {
    readonly attribute DOMString methodName;
    readonly attribute object?   methodDetails;
};

dictionary PaymentMethodChangeEventInit : PaymentRequestUpdateEventInit {
    DOMString methodName = "";
    object?   methodDetails = null;
};
