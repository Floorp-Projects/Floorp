// testing data declation
const defaultMethods = [{
  supportedMethods: "basic-card",
  data: {
    supportedNetworks: ['unionpay', 'visa', 'mastercard', 'amex', 'discover',
                        'diners', 'jcb', 'mir',
    ],
    supportedTypes: ['prepaid', 'debit', 'credit'],
  },
}, {
  supportedMethods: "testing-payment-method",
}];

const defaultDetails = {
  total: {
    label: "Total",
    amount: {
      currency: "USD",
      value: "1.00",
    }
  },
  shippingOptions: [
    {
      id: "NormalShipping",
      label: "NormalShipping",
      amount: {
        currency: "USD",
        value: "10.00",
      },
      selected: false,
    },
    {
      id: "FastShipping",
      label: "FastShipping",
      amount: {
        currency: "USD",
        value: "5.00",
      },
      selected: false,
    },
  ],
};

const defaultOptions = {
  requestPayerName: true,
  requestPayerEmail: false,
  reqeustPayerPhone: false,
  requestShipping: true,
  shippingType: "shipping"
};
