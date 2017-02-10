'use strict';

class PasswordPrompt {
  /**
   * @constructs PasswordPrompt
   */
  constructor(viewport) {
    this.overlayName = 'passwordOverlay';
    this.container = document.getElementById('passwordOverlay');
    this.overlaysParent = document.getElementById('overlayContainer');
    this.label = document.getElementById('passwordText');
    this.input = document.getElementById('password');
    this.submitButton = document.getElementById('passwordSubmit');
    this.cancelButton = document.getElementById('passwordCancel');

    this.updateCallback = null;
    this.reason = null;
    this.active = false;
    this._viewport = viewport;
    // PDFium doesn't return the result of password check. We count the retries
    // to determine whether to show "invalid password" prompt instead.
    // PDFium allows at most 3 times of tries.
    this._passwordTries = 0;
    this._viewport.onPasswordRequest = this.open.bind(this);

    // Attach the event listeners.
    this.submitButton.addEventListener('click', this);
    this.cancelButton.addEventListener('click', this);
    this.input.addEventListener('keydown', this);
  }

  handleEvent(e) {
    switch(e.type) {
    case 'keydown':
      if (e.target == this.input && e.keyCode === KeyEvent.DOM_VK_RETURN) {
        this.verify();
        e.stopPropagation();
      } else if (e.currentTarget == window &&
                 e.keyCode === KeyEvent.DOM_VK_ESCAPE) {
        this.close();
        e.preventDefault();
        e.stopImmediatePropagation();
      }
      break;
    case 'click':
      if (e.target == this.submitButton) {
        this.verify();
      } else if (e.target == this.cancelButton) {
        this.close();
      }
      break;
    }
  }

  open() {
    this.container.classList.remove('hidden');
    this.overlaysParent.classList.remove('hidden');
    window.addEventListener('keydown', this);
    this.active = true;
    let promptKey = this._passwordTries ? 'password_invalid' : 'password_label';
    this._passwordTries++;

    this.input.type = 'password';
    this.input.focus();

    document.l10n.formatValue(promptKey).then(promptString => {
      this.label.textContent = promptString;
    });

  }

  close() {
    this.container.classList.add('hidden');
    this.overlaysParent.classList.add('hidden');
    window.removeEventListener('keydown', this);
    this.active = false;

    this.input.value = '';
    this.input.type = '';
  }

  verify() {
    let password = this.input.value;
    if (password && password.length > 0) {
      this.close();
      this._viewport.verifyPassword(password);
    }
  }
}
