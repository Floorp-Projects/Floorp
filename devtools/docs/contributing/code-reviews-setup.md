# Set up for code reviews

There are two things you need to do before you can get a code review, although you only need to do this once 😃

## Set up to get code reviews in Phabricator

We use an online tool called Phabricator for code reviews. To create an account in Phabricator, you first need the Bugzilla account that you created earlier. If you don't have one, [create it now](../getting-started/bugzilla.md).

---

⚠️ *IMPORTANT:*  ⚠️️️

It's helpful to have the same user name in both Bugzilla and Phabricator, so that people always know how to find you.

Bugzilla's `Real name` field can be edited after the fact, but you cannot change Phabricator's username once the account has been created.

If you added an `:ircnickname` in your Bugzilla's `Real name`, Phabricator will use that to pre-fill the username field when you create the account. **Please double check you like the proposed username, and make any corrections before you register**.

---

Once you understand the above, please [create a Phabricator account](https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html#creating-an-account). 



## Set up to send code for review

There are two ways of doing this (sorry, let us explain!):

* you can use [Arcanist](https://moz-conduit.readthedocs.io/en/latest/arcanist-user.html), which is the official command line tool that accompanies Phabricator, and should be enough for most of the cases.
* or you could use [moz-phab](https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html#using-moz-phab), which is a Mozilla-developed wrapper for Arcanist that makes it work better with the "Mozilla workflow".

**We recommend you use Arcanist** for now, unless you are more experienced and know what you're doing, or want to take advantage of `moz-phab`'s features. You need to install Arcanist for `moz-phab` to work anyway.

If you decide to use `moz-phab`, please be aware that we started using this new tool quite recently, and you might find bugs (or things that don't feel quite right), in which case you could either [have a look at the existing bugs](https://bugzilla.mozilla.org/buglist.cgi?product=Conduit&component=Review%20Wrapper&resolution=---) to see if someone else has encountered this again, or simply [file a bug](https://bugzilla.mozilla.org/enter_bug.cgi?product=Conduit&component=Review%20Wrapper) using your fancy new Bugzilla account 😀
